#include <node.h>
#include <node_buffer.h>
#include <nan.h>
#include <uv.h>
#include <list>

#if defined(_WIN32)
#include <io.h>
#include <memory>
#else
#include <libgen.h>
#endif

#include "genaro.h"

using namespace v8;
using namespace Nan;

class free_env_proxy
{
public:
	Nan::Persistent<v8::Object> persistent;
};

typedef struct
{
	Nan::Callback *progress_callback;
	Nan::Callback *finished_callback;
} transfer_callbacks_t;

typedef struct uploading_task
{
	const char *bucket_id;
	const char *file_name;
} uploading_task_t;

typedef std::list<uploading_task_t> uploading_task_list_t;

// the current uploading tasks.
static uploading_task_list_t uploading_task_list;

typedef struct downloading_task
{
	const char *full_path;
} downloading_task_t;

typedef std::list<downloading_task_t> downloading_task_list_t;

// the current downloading tasks.
static downloading_task_list_t downloading_task_list;

extern "C" void JsonLogger(const char *message, int level, void *handle)
{
	printf("{\"message\": \"%s\", \"level\": %i, \"timestamp\": %" PRIu64 "}\n",
		message, level, genaro_util_timestamp());
}

char *str_concat_many(int count, ...)
{
	int length = 1;

	va_list args;
	va_start(args, count);
	for (int i = 0; i < count; i++) {
		char *item = va_arg(args, char *);
		length += strlen(item);
	}
	va_end(args);

	char *combined = (char *)calloc(length, sizeof(char));
	if (!combined) {
		return NULL;
	}

	va_start(args, count);
	for (int i = 0; i < count; i++) {
		char *item = va_arg(args, char *);
		strcat(combined, item);
	}
	va_end(args);

	return combined;
}

char *RetrieveNewName(const char *fileName, const char *extra)
{
	if (fileName == NULL || strlen(fileName) == 0)
	{
		return NULL;
	}

	int len = strlen(fileName);
	char *retName = NULL;

	int dotIndex = len;
	bool flag = false;
	for (int i = 0; i < len; i++)
	{
		if (!flag)
		{
			if (fileName[i] != '.')
			{
				flag = true;
			}
		}

		if (flag && fileName[i] == '.')
		{
			dotIndex = i;
			break;
		}
	}

	char *left_name = (char *)malloc((dotIndex + 1) * sizeof(char));
	strncpy(left_name, fileName, dotIndex);
	left_name[dotIndex] = '\0';

	char *right_name = NULL;
	if (dotIndex != len)
	{
		right_name = (char *)malloc((len - dotIndex + 2) * sizeof(char));
		right_name[0] = '.';
		for (int i = dotIndex + 2; i < len + 1; i++)
		{
			right_name[i - dotIndex - 1] = fileName[i - 1];
		}
		right_name[len - dotIndex] = '\0';
	}

	if (right_name)
	{
		retName = str_concat_many(3, left_name, extra, right_name);
	}
	else
	{
		retName = str_concat_many(2, left_name, extra);
	}

	free(left_name);
	free(right_name);

	return retName;
}

#ifdef _WIN32
const char *ConvertToWindowsPath(const char *path)
{
	if (path == NULL)
	{
		return NULL;
	}

	int len = strlen(path);
	char *retPath = (char *)malloc((len + 1) * sizeof(char));

	for (int i = 0; i < len; i++)
	{
		if (path[i] != '/')
		{
			retPath[i] = path[i];
		}
		else
		{
			retPath[i] = '\\';
		}
	}
	retPath[len] = '\0';

	return retPath;
}
#endif

Local<Value> IntToGenaroError(int error_code)
{
	if (!error_code)
	{
		return Nan::Null();
	}

	const char *error_msg = genaro_strerror(error_code);
	v8::Local<v8::String> msg = Nan::New(error_msg).ToLocalChecked();
	v8::Local<v8::Value> error = Nan::Error(msg);

	return error;
}

Local<Value> IntToCurlError(int error_code)
{
	const char *error_msg = curl_easy_strerror((CURLcode)error_code);
	v8::Local<v8::String> msg = Nan::New(error_msg).ToLocalChecked();
	return Nan::Error(msg);
}

Local<Value> IntToStatusError(int status_code)
{
	Local<String> error_message;
	switch (status_code)
	{
	case 400:
		error_message = Nan::New("Bad request").ToLocalChecked();
		break;
	case 401:
		error_message = Nan::New("Not authorized").ToLocalChecked();
		break;
	case 404:
		error_message = Nan::New("Resource not found").ToLocalChecked();
		break;
	case 420:
		error_message = Nan::New("Transfer rate limit").ToLocalChecked();
		break;
	case 429:
		error_message = Nan::New("Request rate limited").ToLocalChecked();
		break;
	case 499:
		error_message = Nan::New("No Payment Wallet").ToLocalChecked();
		break;
	case 500:
		error_message = Nan::New("Internal error").ToLocalChecked();
		break;
	case 501:
		error_message = Nan::New("Not implemented").ToLocalChecked();
		break;
	case 503:
		error_message = Nan::New("Service unavailable").ToLocalChecked();
		break;
	default:
		error_message = Nan::New("Unknown status error").ToLocalChecked();
	}
	Local<Value> error = Nan::Error(error_message);
	return error;
}

template <typename ReqType>
bool error_and_status_check(ReqType *req, Local<Value> *error)
{
	if (req->error_code)
	{
		*error = IntToCurlError(req->error_code);
	}
	else if (req->status_code > 399)
	{
		*error = IntToStatusError(req->status_code);
	}
	else
	{
		return true;
	}

	return false;
}

void Timestamp(const v8::FunctionCallbackInfo<Value> &args)
{
	Isolate *isolate = args.GetIsolate();

	uint64_t timestamp = genaro_util_timestamp();
	Local<Number> timestamp_local = Number::New(isolate, (double)timestamp);

	args.GetReturnValue().Set(timestamp_local);
}

void GetInfoCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	json_request_t *req = (json_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;

	v8::Local<v8::Value> error = Nan::Null();
	v8::Local<Value> result = Nan::Null();

	if (error_and_status_check<json_request_t>(req, &error))
	{
		const char *result_str = json_object_to_json_string(req->response);
		v8::Local<v8::String> result_json_string = Nan::New(result_str).ToLocalChecked();
		Nan::JSON NanJSON;
		Nan::MaybeLocal<v8::Value> res = NanJSON.Parse(result_json_string);
		if (!res.IsEmpty())
		{
			result = res.ToLocalChecked();
		}
	}

	Local<Value> argv[] = {
		error,
		result };

	Nan::Call(*callback, 2, argv);

	free(req);
	free(work_req);
}

void GetInfo(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1 || !args[0]->IsFunction())
	{
		return Nan::ThrowError("First argument is expected to be a function");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Callback *callback = new Nan::Callback(args[0].As<Function>());

	genaro_bridge_get_info(env, (void *)callback, GetInfoCallback);
}

Local<Date> StrToDate(const char *dateStr)
{
	Local<Date> tmp = Nan::New<Date>(0).ToLocalChecked();
	v8::Local<v8::Function> cons = v8::Local<v8::Function>::Cast(
		Nan::Get(tmp, Nan::New("constructor").ToLocalChecked()).ToLocalChecked());
	const int argc = 1;
	v8::Local<v8::Value> argv[argc] = { Nan::New(dateStr).ToLocalChecked() };
	v8::Local<v8::Date> date = v8::Local<v8::Date>::Cast(
		Nan::NewInstance(cons, argc, argv).ToLocalChecked());
	return date;
}

void GetBucketsCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	get_buckets_request_t *req = (get_buckets_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;
	Local<Value> buckets_value = Nan::Null();
	Local<Value> error = Nan::Null();

	if (error_and_status_check<get_buckets_request_t>(req, &error))
	{
		Local<Array> buckets_array = Nan::New<Array>();
		for (uint32_t i = 0; i < req->total_buckets; i++)
		{
			Local<Object> bucket = Nan::New<Object>();
			bucket->Set(Nan::New("name").ToLocalChecked(), Nan::New(req->buckets[i].name).ToLocalChecked());
			bucket->Set(Nan::New("created").ToLocalChecked(), StrToDate(req->buckets[i].created));
			bucket->Set(Nan::New("id").ToLocalChecked(), Nan::New(req->buckets[i].id).ToLocalChecked());
			bucket->Set(Nan::New("bucketId").ToLocalChecked(), Nan::New(req->buckets[i].bucketId).ToLocalChecked());
			bucket->Set(Nan::New("decrypted").ToLocalChecked(), Nan::New<Boolean>(req->buckets[i].decrypted));
			bucket->Set(Nan::New("limitStorage").ToLocalChecked(), Nan::New((double)req->buckets[i].limitStorage));
			bucket->Set(Nan::New("usedStorage").ToLocalChecked(), Nan::New((double)req->buckets[i].usedStorage));
			bucket->Set(Nan::New("timeStart").ToLocalChecked(), Nan::New((double)req->buckets[i].timeStart));
			bucket->Set(Nan::New("timeEnd").ToLocalChecked(), Nan::New((double)req->buckets[i].timeEnd));
			buckets_array->Set(i, bucket);
		}
		buckets_value = buckets_array;
	}

	Local<Value> argv[] = {
		error,
		buckets_value };

	Nan::Call(*callback, 2, argv);

	free(req);
	free(work_req);
}

void GetBuckets(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1 || !args[0]->IsFunction())
	{
		return Nan::ThrowError("First argument is expected to be a function");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Callback *callback = new Nan::Callback(args[0].As<Function>());

	genaro_bridge_get_buckets(env, (void *)callback, GetBucketsCallback);
}

void ListFilesCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	list_files_request_t *req = (list_files_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;
	Local<Value> files_value = Nan::Null();
	Local<Value> error = Nan::Null();

	if (error_and_status_check<list_files_request_t>(req, &error))
	{
		Local<Array> files_array = Nan::New<Array>();
		for (uint32_t i = 0; i < req->total_files; i++)
		{
			Local<Object> file = Nan::New<Object>();
			file->Set(Nan::New("filename").ToLocalChecked(), Nan::New(req->files[i].filename).ToLocalChecked());
			file->Set(Nan::New("mimetype").ToLocalChecked(), Nan::New(req->files[i].mimetype).ToLocalChecked());
			file->Set(Nan::New("id").ToLocalChecked(), Nan::New(req->files[i].id).ToLocalChecked());
			file->Set(Nan::New("size").ToLocalChecked(), Nan::New((double)(req->files[i].size)));
			file->Set(Nan::New("created").ToLocalChecked(), StrToDate(req->files[i].created));

			if(req->files[i].rsaKey && req->files[i].rsaCtr)
			{
				file->Set(Nan::New("rsaKey").ToLocalChecked(), Nan::New(req->files[i].rsaKey).ToLocalChecked());
				file->Set(Nan::New("rsaCtr").ToLocalChecked(), Nan::New(req->files[i].rsaCtr).ToLocalChecked());
			}
			files_array->Set(i, file);
		}
		files_value = files_array;
	}

	Local<Value> argv[] = {
		error,
		files_value };

	Nan::Call(*callback, 2, argv);

	free(req);
	free(work_req);
}

void ListFiles(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 2 || !args[1]->IsFunction())
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String str(args[0]);
	const char *bucket_id = *str;
	const char *bucket_id_dup = strdup(bucket_id);

	Nan::Callback *callback = new Nan::Callback(args[1].As<Function>());

	genaro_bridge_list_files(env, bucket_id_dup, (void *)callback, ListFilesCallback);
}

void CreateBucketCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	create_bucket_request_t *req = (create_bucket_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;

	Local<Value> bucket_value = Nan::Null();
	Local<Value> error = Nan::Null();

	if (error_and_status_check<create_bucket_request_t>(req, &error))
	{
		Local<Object> bucket_object = Nan::To<Object>(Nan::New<Object>()).ToLocalChecked();
		bucket_object->Set(Nan::New("name").ToLocalChecked(), Nan::New(req->bucket->name).ToLocalChecked());
		bucket_object->Set(Nan::New("id").ToLocalChecked(), Nan::New(req->bucket->id).ToLocalChecked());
		bucket_object->Set(Nan::New("decrypted").ToLocalChecked(), Nan::New<Boolean>(req->bucket->decrypted));
		bucket_value = bucket_object;
	}

	Local<Value> argv[] = {
		error,
		bucket_value };

	Nan::Call(*callback, 2, argv);

	free(req);
	free(work_req);
}

void CreateBucket(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 2 || !args[1]->IsFunction())
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String str(args[0]);
	const char *name = *str;
	const char *name_dup = strdup(name);

	Nan::Callback *callback = new Nan::Callback(args[1].As<Function>());

	genaro_bridge_create_bucket(env, name_dup, (void *)callback, CreateBucketCallback);
}

void DeleteBucketCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	json_request_t *req = (json_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;
	Local<Value> error = Nan::Null();

	error_and_status_check<json_request_t>(req, &error);

	Local<Value> argv[] = {
		error };

	Nan::Call(*callback, 1, argv);

	free(req);
	free(work_req);
}

void DeleteBucket(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 2 || !args[1]->IsFunction())
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String str(args[0]);
	const char *id = *str;
	const char *id_dup = strdup(id);

	Nan::Callback *callback = new Nan::Callback(args[1].As<Function>());

	genaro_bridge_delete_bucket(env, id_dup, (void *)callback, DeleteBucketCallback);
}

void RenameBucketCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	rename_bucket_request_t *req = (rename_bucket_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;

	Local<Value> error = Nan::Null();

	error_and_status_check<rename_bucket_request_t>(req, &error);

	Local<Value> argv[] = {
		error };

	Nan::Call(*callback, 1, argv);

	free(req);
	free(work_req);
}

void RenameBucket(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 3 || !args[2]->IsFunction())
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String id_str(args[0]);
	const char *id = *id_str;
	const char *id_dup = strdup(id);

	Nan::Utf8String name_str(args[1]);
	const char *name = *name_str;
	const char *name_dup = strdup(name);

	Nan::Callback *callback = new Nan::Callback(args[2].As<Function>());

	genaro_bridge_rename_bucket(env, id_dup, name_dup, (void *)callback, RenameBucketCallback);
}

void RemoveUploadingTask(const char *bucket_id, const char *file_name)
{
	uploading_task_list_t::iterator iter = uploading_task_list.begin();

	while (iter != uploading_task_list.end())
	{
		if (!strcmp(iter->file_name, file_name) &&
			!strcmp(iter->bucket_id, bucket_id))
		{
			free((void *)iter->bucket_id);
			free((void *)iter->file_name);
			uploading_task_list.erase(iter);
			return;
		}

		++iter;
	}
}

bool IsUploading(const char *bucket_id, const char *file_name)
{
	uploading_task_list_t::iterator iter = uploading_task_list.begin();

	while (iter != uploading_task_list.end())
	{
		if (!strcmp(iter->bucket_id, bucket_id) &&
			!strcmp(iter->file_name, file_name))
		{
			return true;
		}

		++iter;
	}

	return false;
}

void RemoveDownloadingTask(const char *full_path)
{
	downloading_task_list_t::iterator iter = downloading_task_list.begin();

	while (iter != downloading_task_list.end())
	{
	#ifdef _WIN32
		const char *converted_full_path = ConvertToWindowsPath(full_path);
		if (!_stricmp(iter->full_path, converted_full_path))
	#else
		if (!strcmp(iter->full_path, full_path))
	#endif
		{
		#ifdef _WIN32
			free((void *)converted_full_path);
		#endif

			free((void *)iter->full_path);
			downloading_task_list.erase(iter);

			return;
		}

	#ifdef _WIN32
		free((void *)converted_full_path);
	#endif

		++iter;
	}
}

bool IsDownloading(const char *full_path)
{
	downloading_task_list_t::iterator iter = downloading_task_list.begin();

	while (iter != downloading_task_list.end())
	{
	#ifdef _WIN32
		const char *converted_full_path = ConvertToWindowsPath(full_path);
		if (!_stricmp(iter->full_path, converted_full_path))
	#else
		if (!strcmp(iter->full_path, full_path))
	#endif
		{
		#ifdef _WIN32
			free((void *)converted_full_path);
		#endif

			return true;
		}

	#ifdef _WIN32
		free((void *)converted_full_path);
	#endif

		++iter;
	}

	return false;
}

void StoreFileFinishedCallback(const char *bucket_id, const char *file_name, int status, char *file_id, uint64_t file_bytes, char *sha256_of_encrypted, void *handle)
{
	Nan::HandleScope scope;

	RemoveUploadingTask(bucket_id, file_name);

	transfer_callbacks_t *upload_callbacks = (transfer_callbacks_t *)handle;
	Nan::Callback *callback = upload_callbacks->finished_callback;

	Local<Value> file_id_local = Nan::Null();
	Local<Value> file_bytes_local = Nan::Null();
	Local<Value> sha256_of_encrypted_local = Nan::Null();
	if (status == 0)
	{
		file_id_local = Nan::New(file_id).ToLocalChecked();
		file_bytes_local = Nan::New((double)file_bytes);
		sha256_of_encrypted_local = Nan::New(sha256_of_encrypted).ToLocalChecked();
	}

	Local<Value> error = IntToGenaroError(status);

	Local<Value> argv[] = {
		error,
		file_id_local,
		file_bytes_local,
		sha256_of_encrypted_local };

	Nan::Call(*callback, 4, argv);

	free(file_id);
	free(sha256_of_encrypted);
}

void StoreFileProgressCallback(double progress, uint64_t file_bytes, void *handle)
{
	Nan::HandleScope scope;

	transfer_callbacks_t *upload_callbacks = (transfer_callbacks_t *)handle;
	Nan::Callback *callback = upload_callbacks->progress_callback;

	Local<Number> progress_local = Nan::New(progress);
	Local<Number> file_bytes_local = Nan::New((double)file_bytes);

	Local<Value> argv[] = {
		progress_local,
		file_bytes_local };

	Nan::Call(*callback, 2, argv);
}

template <class StateType>
void StateStatusErrorGetter(Local<String> property, const Nan::PropertyCallbackInfo<Value> &info)
{
	Local<Object> self = info.Holder();
	StateType *state = (StateType *)self->GetAlignedPointerFromInternalField(0);
	Local<Value> error = IntToGenaroError(state->error_status);
	info.GetReturnValue().Set(error);
}

//add on 2018.5.9
#if defined(_WIN32)
std::unique_ptr<char[]> EncodingConvert(const char* strIn, int sourceCodepage, int targetCodepage)
{
	int unicodeLen = MultiByteToWideChar(sourceCodepage, 0, strIn, -1, nullptr, 0);

	wchar_t * pUnicode = new wchar_t[unicodeLen + 1];
	memset(pUnicode, 0, (unicodeLen + 1) * sizeof(wchar_t));

	MultiByteToWideChar(sourceCodepage, 0, strIn, -1, pUnicode, unicodeLen);

	char * pTargetData = nullptr;
	int targetLen = WideCharToMultiByte(targetCodepage, 0, pUnicode, -1, pTargetData, 0, nullptr, nullptr);

	pTargetData = new char[targetLen + 1];
	memset(pTargetData, 0, targetLen + 1);

	WideCharToMultiByte(targetCodepage, 0, pUnicode, -1, pTargetData, targetLen, nullptr, nullptr);

	delete[] pUnicode;
	return std::unique_ptr<char[]>(pTargetData);
}
#endif

void GenerateEncryptionInfo(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String bucket_id_str(args[0]);
	const char *bucket_id = *bucket_id_str;

	genaro_encryption_info_t *encryption_info = genaro_generate_encryption_info(env, NULL, bucket_id);

	if (encryption_info)
	{
		Local<Object> encryption = Nan::New<Object>();
		encryption->Set(Nan::New("index").ToLocalChecked(), Nan::New(encryption_info->index).ToLocalChecked());
		free(encryption_info->index);
		encryption->Set(Nan::New("key").ToLocalChecked(), Nan::New(encryption_info->key_ctr_as_str->key_as_str).ToLocalChecked());
		free((void *)encryption_info->key_ctr_as_str->key_as_str);
		encryption->Set(Nan::New("ctr").ToLocalChecked(), Nan::New(encryption_info->key_ctr_as_str->ctr_as_str).ToLocalChecked());
		free((void *)encryption_info->key_ctr_as_str->ctr_as_str);
		free(encryption_info->key_ctr_as_str);
		free(encryption_info);

		args.GetReturnValue().Set(encryption);
	}
}

// void EncryptData(const Nan::FunctionCallbackInfo<Value> &args)
// {
// 	if (args.Length() != 1)
// 	{
// 		return Nan::ThrowError("Unexpected arguments");
// 	}
// 	if (args.This()->InternalFieldCount() != 1)
// 	{
// 		return Nan::ThrowError("Environment not available for instance");
// 	}

// 	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
// 	if (!env)
// 	{
// 		return Nan::ThrowError("Environment is not initialized");
// 	}

// 	Nan::Utf8String bucket_id_str(args[0]);
// 	const char *bucket_id = *bucket_id_str;

// 	genaro_encryption_info_t *encryption_info = genaro_generate_encryption_info(env, NULL, bucket_id);

// 	if (encryption_info)
// 	{
// 		Local<Object> encryption = Nan::New<Object>();
// 		encryption->Set(Nan::New("index").ToLocalChecked(), Nan::New(encryption_info->index).ToLocalChecked());
// 		free(encryption_info->index);
// 		encryption->Set(Nan::New("key").ToLocalChecked(), Nan::New(encryption_info->key_ctr_as_str->key_as_str).ToLocalChecked());
// 		free((void *)encryption_info->key_ctr_as_str->key_as_str);
// 		encryption->Set(Nan::New("ctr").ToLocalChecked(), Nan::New(encryption_info->key_ctr_as_str->ctr_as_str).ToLocalChecked());
// 		free((void *)encryption_info->key_ctr_as_str->ctr_as_str);
// 		free(encryption_info->key_ctr_as_str);
// 		free(encryption_info);

// 		args.GetReturnValue().Set(encryption);
// 	}
// }

void StoreFile(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 4)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String bucket_id_str(args[0]);
	const char *bucket_id = *bucket_id_str;
	const char *bucket_id_dup = strdup(bucket_id);

	Nan::Utf8String file_or_data_str(args[1]);
	const char *file_or_data = *file_or_data_str;

 	v8::Local<v8::Boolean> is_file_path_local = v8::Local<v8::Boolean>::Cast(args[2]); 
    bool is_file_path = is_file_path_local->BooleanValue();

	v8::Local<v8::Object> options = args[3].As<v8::Object>();

	transfer_callbacks_t *upload_callbacks = static_cast<transfer_callbacks_t *>(malloc(sizeof(transfer_callbacks_t)));

	upload_callbacks->progress_callback = new Nan::Callback(options->Get(Nan::New("progressCallback").ToLocalChecked()).As<Function>());
	upload_callbacks->finished_callback = new Nan::Callback(options->Get(Nan::New("finishedCallback").ToLocalChecked()).As<Function>());

	Nan::Utf8String file_name_str(options->Get(Nan::New("filename").ToLocalChecked()).As<v8::String>());
	const char *file_name = *file_name_str;
	const char *file_name_dup = strdup(file_name);

	if (IsUploading(bucket_id_dup, file_name_dup))
	{
		v8::Local<v8::String> msg = Nan::New("File is already uploading").ToLocalChecked();
		v8::Local<v8::Value> error = Nan::Error(msg);

		Local<Value> argv[] = {
			error,
			Nan::Null() };

		Nan::Call(*(upload_callbacks->finished_callback), 2, argv);

		return;
	}

	const char *file_path = NULL;

	if(is_file_path)
	{
		file_path = file_or_data;
	}
	else
	{
		const char *tmp_path = NULL;
		if (getenv("GENARO_TEMP") && !access(getenv("GENARO_TEMP"), F_OK)) {
			tmp_path = getenv("GENARO_TEMP");
		#ifdef _WIN32
		} else if (getenv("TEMP") && !access(getenv("TEMP"), F_OK)) {
			tmp_path = getenv("TEMP");
		#else
		} else if ("/tmp" && !access("/tmp", F_OK)) {
			tmp_path = "/tmp";
		#endif
		} else {
			return Nan::ThrowError("Get temp path failed");
		}

	#ifdef _WIN32
		const char *path_slash = "\\";
	#else
		const char *path_slash = "/";
	#endif
		char *temp_file_path = str_concat_many(3, tmp_path, path_slash, "tmp-XXXXXX");

		int fd = -1;
	#ifdef _WIN32
		int err = _mktemp_s(temp_file_path, strlen(temp_file_path) + 1);
		if(!err)
		{
			fd = open(temp_file_path, O_WRONLY | O_CREAT);
		}
	#else
		fd = mkstemp(temp_file_path);
	#endif

		if(fd == -1)
		{
			return Nan::ThrowError("Create temp file failed");
		}

		size_t len = strlen(file_or_data);
		if(write(fd, file_or_data, len) != len)
		{
			close(fd);
			return Nan::ThrowError("Write data to file failed");
		}
		close(fd);
		file_path = temp_file_path;
	}

	//convert to ANSI encoding on Win32, add on 2018.5.9
#if defined(_WIN32)
	std::unique_ptr<char[]> u_p = EncodingConvert(file_path, CP_UTF8, CP_ACP);
	file_path = u_p.get();
#endif

	FILE *fd = fopen(file_path, "r");

	if (!fd)
	{
		v8::Local<v8::String> msg = Nan::New("Unable to open file").ToLocalChecked();
		v8::Local<v8::Value> error = Nan::Error(msg);

		Local<Value> argv[] = {
			error,
			Nan::Null() };

		Nan::Call(*(upload_callbacks->finished_callback), 2, argv);

		return;
	}

	genaro_upload_opts_t upload_opts = {};
	upload_opts.prepare_frame_limit = 1,
	upload_opts.push_frame_limit = 64;
	upload_opts.push_shard_limit = 64;
	upload_opts.rs = true;
	upload_opts.bucket_id = bucket_id_dup;
	upload_opts.file_name = file_name_dup;
	upload_opts.fd = fd;

	Nan::Utf8String index_str(options->Get(Nan::New("index").ToLocalChecked()));
	const char *index = *index_str;
	const char *index_dup = strdup(index);

	Nan::Utf8String key_str(options->Get(Nan::New("key").ToLocalChecked()));
	const char *key = *key_str;
	const char *key_dup = strdup(key);

	Nan::Utf8String ctr_str(options->Get(Nan::New("ctr").ToLocalChecked()));
	const char *ctr = *ctr_str;
	const char *ctr_dup = strdup(ctr);

	genaro_key_ctr_as_str_t *key_ctr = (genaro_key_ctr_as_str_t *)malloc(sizeof(genaro_key_ctr_as_str_t));
	key_ctr->key_as_str = key_dup;
	key_ctr->ctr_as_str = ctr_dup;

	Nan::Utf8String rsa_key_str(options->Get(Nan::New("rsaKey").ToLocalChecked()));
	const char *rsa_key = *rsa_key_str;
	const char *rsa_key_dup = strdup(rsa_key);

	Nan::Utf8String rsa_ctr_str(options->Get(Nan::New("rsaCtr").ToLocalChecked()));
	const char *rsa_ctr = *rsa_ctr_str;
	const char *rsa_ctr_dup = strdup(rsa_ctr);

	genaro_key_ctr_as_str_t *rsa_key_ctr_as_str = (genaro_key_ctr_as_str_t *)malloc(sizeof(genaro_key_ctr_as_str_t));
	rsa_key_ctr_as_str->key_as_str = rsa_key_dup;
	rsa_key_ctr_as_str->ctr_as_str = rsa_ctr_dup;

	genaro_upload_state_t *state;

	state = genaro_bridge_store_file(env, &upload_opts,
		index_dup,
		key_ctr,
		rsa_key_ctr_as_str,
		(void *)upload_callbacks,
		StoreFileProgressCallback,
		StoreFileFinishedCallback);

	if (!state)
	{
		return Nan::ThrowError("Unable to create upload state");
	}

	if (state->error_status)
	{
		return Nan::ThrowError("Unable to queue file upload");
	}

	uploading_task_t task;
	task.bucket_id = strdup(state->bucket_id);
	task.file_name = strdup(state->file_name);

	uploading_task_list.push_back(task);

	Isolate *isolate = args.GetIsolate();
	Local<ObjectTemplate> state_template = ObjectTemplate::New(isolate);
	state_template->SetInternalFieldCount(1);

	Local<Object> state_local = state_template->NewInstance();
	state_local->SetAlignedPointerInInternalField(0, state);
	Nan::SetAccessor(state_local, Nan::New("error_status").ToLocalChecked(),
		StateStatusErrorGetter<genaro_upload_state_t>);

	args.GetReturnValue().Set(state_local);
}

void StoreFileCancel(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1)
	{
		return Nan::ThrowError("Unexpected arguments");
	}

	Local<Object> state_local = args[0].As<Object>();
	if (state_local->IsNullOrUndefined())
	{
		return Nan::ThrowError("Unexpected arguments");
	}

	genaro_upload_state_t *state = (genaro_upload_state_t *)state_local->GetAlignedPointerFromInternalField(0);
	genaro_bridge_store_file_cancel(state);
}

void ResolveFileCancel(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1)
	{
		return Nan::ThrowError("Unexpected arguments");
	}

	Local<Object> state_local = args[0].As<Object>();
	if (state_local->IsNullOrUndefined())
	{
		return Nan::ThrowError("Unexpected arguments");
	}

	genaro_download_state_t *state = (genaro_download_state_t *)state_local->GetAlignedPointerFromInternalField(0);
	genaro_bridge_resolve_file_cancel(state);
}

void ResolveFileFinishedCallback(int status, const char *file_name, const char *temp_file_name, FILE *fd, uint64_t file_bytes, char *sha256, void *handle)
{
	Nan::HandleScope scope;

	RemoveDownloadingTask(file_name);

	if(fd) {
		fclose(fd);
	}

	int rename_failed = 0;

	// download success, remove the original file, and rename
	// the downloaded file to the same file name.
	if (status == 0)
	{
		const char *final_file_name = strdup(file_name);

		bool getname_failed = true;

		// original file exists
		if (access(final_file_name, F_OK) != -1)
		{
			// delete it
			int ret = unlink(final_file_name);

			// failed to delete
			if (ret != 0)
			{
			#ifndef _WIN32
				char *path_name = dirname((char *)final_file_name);
				char *file_name = basename((char *)final_file_name);
			#else
				char drive[_MAX_DRIVE];
				char dir[_MAX_DIR];
				char fname[_MAX_FNAME];
				char ext[_MAX_EXT];

				_splitpath(final_file_name, drive, dir, fname, ext);
				char *path_name = str_concat_many(2, drive, dir);
				char *file_name = str_concat_many(2, fname, ext);
			#endif

				int index = 1;
				char temp_str[5];

				do
				{
					sprintf(temp_str, " (%d)", index);
					char *temp_file_name = RetrieveNewName(file_name, temp_str);
					if (temp_file_name == NULL)
					{
						break;
					}

					free((void *)final_file_name);
					final_file_name = str_concat_many(2, path_name, temp_file_name);

					free(temp_file_name);

					if (access(final_file_name, F_OK) == -1)
					{
						getname_failed = false;
						break;
					}
				} while (++index < 10);
			}
			else
			{
				getname_failed = false;
			}
		}
		else
		{
			getname_failed = false;
		}

		if (!getname_failed)
		{
			rename_failed = rename(temp_file_name, final_file_name);
			if(rename_failed)
			{
				unlink(temp_file_name);
			}
		}
		else
		{
			rename_failed = 1;
			unlink(temp_file_name);
		}

		free((void *)final_file_name);
	}
	else
	{
		// download failed, delete the temp file.
		unlink(temp_file_name);
	}

	free((void *)file_name);
	free((void *)temp_file_name);

	transfer_callbacks_t *download_callbacks = (transfer_callbacks_t *)handle;
	Nan::Callback *callback = download_callbacks->finished_callback;

	Local<Value> file_bytes_local = Nan::Null();
	Local<Value> sha256_local = Nan::Null();
	if (status == 0)
	{
		file_bytes_local = Nan::New((double)file_bytes);
		sha256_local = Nan::New(sha256).ToLocalChecked();
	}

	Local<Value> error = Nan::Null();
	if(rename_failed)
	{
		v8::Local<v8::String> msg = Nan::New("File rename error").ToLocalChecked();
		error = Nan::Error(msg);
	}
	else
	{
		error = IntToGenaroError(status);
	}

	Local<Value> argv[] = {
		error,
		file_bytes_local,
		sha256_local };

	Nan::Call(*callback, 3, argv);

	free(sha256);
}

void ResolveFileProgressCallback(double progress, uint64_t file_bytes, void *handle)
{
	Nan::HandleScope scope;

	transfer_callbacks_t *download_callbacks = (transfer_callbacks_t *)handle;
	Nan::Callback *callback = download_callbacks->progress_callback;

	Local<Number> progress_local = Nan::New(progress);
	Local<Number> file_bytes_local = Nan::New((double)file_bytes);

	Local<Value> argv[] = {
		progress_local,
		file_bytes_local };

	Nan::Call(*callback, 2, argv);
}

void ResolveFile(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 4)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String bucket_id_str(args[0]);
	const char *bucket_id = *bucket_id_str;
	const char *bucket_id_dup = strdup(bucket_id);

	Nan::Utf8String file_id_str(args[1]);
	const char *file_id = *file_id_str;
	const char *file_id_dup = strdup(file_id);

	Nan::Utf8String file_path_str(args[2]);
	const char *file_path = *file_path_str;

	//convert to ANSI encoding on Win32, add on 2018.5.9
#if defined(_WIN32)
	std::unique_ptr<char[]> u_p = EncodingConvert(file_path, CP_UTF8, CP_ACP);
	const char *file_path_dup = strdup(u_p.get());
#else
	const char *file_path_dup = strdup(file_path);
#endif

	v8::Local<v8::Object> options = args[3].As<v8::Object>();

	bool hasKey = Nan::HasOwnProperty(options, Nan::New("key").ToLocalChecked()).FromJust();
	bool hasCtr = Nan::HasOwnProperty(options, Nan::New("ctr").ToLocalChecked()).FromJust();

	const char *key = NULL;
	const char *ctr = NULL;
	const char *key_dup = NULL;
	const char *ctr_dup = NULL;
	if(hasKey && hasCtr)
	{
		Nan::Utf8String key_str(options->Get(Nan::New("key").ToLocalChecked()));
		key = *key_str;

		if(key && key[0] != '\0')
		{
			key_dup = strdup(key);
		}

		Nan::Utf8String ctr_str(options->Get(Nan::New("ctr").ToLocalChecked()));
		ctr = *ctr_str;

		if(ctr && ctr[0] != '\0')
		{
			ctr_dup = strdup(ctr);
		}
	}

	genaro_key_ctr_as_str_t *key_ctr_as_str = NULL;

	if(key_dup && ctr_dup)
	{
		key_ctr_as_str = (genaro_key_ctr_as_str_t *)malloc(sizeof(genaro_key_ctr_as_str_t));
		key_ctr_as_str->key_as_str = key_dup;
		key_ctr_as_str->ctr_as_str = ctr_dup;
	}

	transfer_callbacks_t *download_callbacks = static_cast<transfer_callbacks_t *>(malloc(sizeof(transfer_callbacks_t)));

	download_callbacks->progress_callback = new Nan::Callback(options->Get(Nan::New("progressCallback").ToLocalChecked()).As<Function>());
	download_callbacks->finished_callback = new Nan::Callback(options->Get(Nan::New("finishedCallback").ToLocalChecked()).As<Function>());

	if (IsDownloading(file_path_dup))
	{
		v8::Local<v8::String> msg = Nan::New("File is already downloading").ToLocalChecked();
		v8::Local<v8::Value> error = Nan::Error(msg);
		Local<Value> argv[] = {
			error };

		Nan::Call(*(download_callbacks->finished_callback), 1, argv);

		return;
	}

	Nan::MaybeLocal<Value> overwriteOption = options->Get(Nan::New("overwrite").ToLocalChecked());

	bool overwrite = false;
	if (!overwriteOption.IsEmpty())
	{
		overwrite = To<bool>(overwriteOption.ToLocalChecked()).FromJust();
	}

	Nan::MaybeLocal<Value> decryptOption = options->Get(Nan::New("decrypt").ToLocalChecked());

	bool decrypt = true;
	if (!decryptOption.IsEmpty())
	{
		decrypt = To<bool>(decryptOption.ToLocalChecked()).FromJust();
	}

	FILE *fd = NULL;

	if (access(file_path_dup, F_OK) != -1)
	{
		if (!overwrite)
		{
			v8::Local<v8::String> msg = Nan::New("File already exists").ToLocalChecked();
			v8::Local<v8::Value> error = Nan::Error(msg);
			Local<Value> argv[] = {
				error };

			Nan::Call(*(download_callbacks->finished_callback), 1, argv);

			return;
		}
	}

	const char *temp_file_name = str_concat_many(2, file_path_dup, ".genarotmp");

	fd = fopen(temp_file_name, "w+");

	if (fd == NULL)
	{
		v8::Local<v8::String> msg = Nan::New(strerror(errno)).ToLocalChecked();
		v8::Local<v8::Value> error = Nan::Error(msg);
		Local<Value> argv[] = {
			error };

		Nan::Call(*(download_callbacks->finished_callback), 1, argv);

		return;
	}

	genaro_download_state_t *state = genaro_bridge_resolve_file(env,
																bucket_id_dup,
																file_id_dup,
																key_ctr_as_str,
																file_path_dup,
																temp_file_name,
																fd,
																decrypt,
																(void *)download_callbacks,
																ResolveFileProgressCallback,
																ResolveFileFinishedCallback);
	if (!state)
	{
		return Nan::ThrowError("Unable to create download state");
	}

	if (state->error_status)
	{
		return Nan::ThrowError("Unable to queue file download");
	}

	downloading_task_t task;
#ifdef _WIN32
	task.full_path = ConvertToWindowsPath(state->file_name);
#else
	task.full_path = strdup(state->file_name);
#endif

	downloading_task_list.push_back(task);

	Isolate *isolate = args.GetIsolate();
	Local<ObjectTemplate> state_template = ObjectTemplate::New(isolate);
	state_template->SetInternalFieldCount(1);

	Local<Object> state_local = state_template->NewInstance();
	state_local->SetAlignedPointerInInternalField(0, state);
	Nan::SetAccessor(state_local, Nan::New("error_status").ToLocalChecked(),
		StateStatusErrorGetter<genaro_download_state_t>);

	args.GetReturnValue().Set(state_local);
}

// decrypt the downloaded but not decrypted file
void DecryptFile(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 3)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String file_path_str(args[0]);
	const char *file_path = *file_path_str;

	//convert to ANSI encoding on Win32
#if defined(_WIN32)
	std::unique_ptr<char[]> u_p = EncodingConvert(file_path, CP_UTF8, CP_ACP);
	file_path = u_p.get();
#endif

	Nan::Utf8String key_str(args[1]);
	const char *key = *key_str;
	Nan::Utf8String ctr_str(args[2]);
	const char *ctr = *ctr_str;

	genaro_key_ctr_as_str_t *key_ctr_as_str = NULL;
	key_ctr_as_str = (genaro_key_ctr_as_str_t *)malloc(sizeof(genaro_key_ctr_as_str_t));
	key_ctr_as_str->key_as_str = key;
	key_ctr_as_str->ctr_as_str = ctr;

	char *decryptedMeta = genaro_decrypt_file(env, file_path, key_ctr_as_str);
	free((void *)key_ctr_as_str);

	if(!decryptedMeta) {
		return;
	}

	// return the decrypted meta
	args.GetReturnValue().Set(Nan::New(decryptedMeta).ToLocalChecked());
	free(decryptedMeta);
}

// TODO: this is the same as DeleteBucketCallback; refactor
void DeleteFileCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	json_request_t *req = (json_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;
	Local<Value> error = Nan::Null();

	error_and_status_check<json_request_t>(req, &error);

	Local<Value> argv[] = {
		error };

	Nan::Call(*callback, 1, argv);

	free(req);
	free(work_req);
}

void DeleteFile(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 3 || !args[2]->IsFunction())
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String bucket_id_str(args[0]);
	const char *bucket_id = *bucket_id_str;
	const char *bucket_id_dup = strdup(bucket_id);

	Nan::Utf8String file_id_str(args[1]);
	const char *file_id = *file_id_str;
	const char *file_id_dup = strdup(file_id);

	Nan::Callback *callback = new Nan::Callback(args[2].As<Function>());

	genaro_bridge_delete_file(env, bucket_id_dup, file_id_dup, (void *)callback, DeleteFileCallback);
}

void EncryptMeta(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String meta_str(args[0]);
	const char *meta = *meta_str;

	char *encrypted_meta = genaro_encrypt_meta(env, meta);

	if (encrypted_meta)
	{
		// return the encrypted meta.
		args.GetReturnValue().Set(Nan::New(encrypted_meta).ToLocalChecked());
	}
}

void EncryptMetaToFile(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 2)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String meta_str(args[0]);
	const char *meta = *meta_str;

	char *encrypted_meta = genaro_encrypt_meta(env, meta);

	Nan::Utf8String file_path_str(args[1]);
	const char *file_path = *file_path_str;

	//convert to ANSI encoding on Win32
#if defined(_WIN32)
	std::unique_ptr<char[]> u_p = EncodingConvert(file_path, CP_UTF8, CP_ACP);
	file_path = u_p.get();
#endif

	bool ret = false;
	if (encrypted_meta) {
		FILE *fd = fopen(file_path, "w+");
		if(fd) {
			fwrite(encrypted_meta, strlen(encrypted_meta), sizeof(char), fd);
			fclose(fd);
			ret = true;
		}
	}

	args.GetReturnValue().Set(Nan::New(ret));
}

void DecryptMeta(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String encrypted_meta_str(args[0]);
	const char *encrypted_meta = *encrypted_meta_str;

	char *decrypted_meta = genaro_decrypt_meta(env, encrypted_meta);

	if (decrypted_meta)
	{
		// return the decrypted meta.
		args.GetReturnValue().Set(Nan::New(decrypted_meta).ToLocalChecked());
	}
}

void DecryptMetaFromFile(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.Length() != 1)
	{
		return Nan::ThrowError("Unexpected arguments");
	}
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	Nan::Utf8String file_path_str(args[0]);
	const char *file_path = *file_path_str;

	//convert to ANSI encoding on Win32
#if defined(_WIN32)
	std::unique_ptr<char[]> u_p = EncodingConvert(file_path, CP_UTF8, CP_ACP);
	file_path = u_p.get();
#endif

	FILE *fp;
    fp = fopen(file_path, "r");
    if (fp == NULL) {
		return;
    }

    fseek(fp, 0, SEEK_END);
    size_t fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *buffer = (char *)calloc(fsize + 1, sizeof(char));
    if (buffer == NULL) {
		return;
    }

    size_t read_blocks = 0;
    while ((!feof(fp)) && (!ferror(fp))) {
        read_blocks = fread(buffer + read_blocks, 1, fsize, fp);
        if (read_blocks <= 0) {
            break;
        }
    }

    int error = ferror(fp);
    fclose(fp);

    if (error) {
		free(buffer);
		return;
    }

	char *decrypted_meta = genaro_decrypt_meta(env, buffer);
	free(buffer);

	if(decrypted_meta) {
		args.GetReturnValue().Set(Nan::New(decrypted_meta).ToLocalChecked());
	}
}

void RegisterCallback(uv_work_t *work_req, int status)
{
	Nan::HandleScope scope;

	json_request_t *req = (json_request_t *)work_req->data;

	Nan::Callback *callback = (Nan::Callback *)req->handle;

	v8::Local<v8::Value> error = Nan::Null();
	v8::Local<Value> result = Nan::Null();

	if (error_and_status_check<json_request_t>(req, &error))
	{
		const char *result_str = json_object_to_json_string(req->response);
		v8::Local<v8::String> result_json_string = Nan::New(result_str).ToLocalChecked();
		Nan::JSON NanJSON;
		Nan::MaybeLocal<v8::Value> res = NanJSON.Parse(result_json_string);
		if (!res.IsEmpty())
		{
			result = res.ToLocalChecked();
		}
	}

	Local<Value> argv[] = {
		error,
		result };

	Nan::Call(*callback, 2, argv);

	free(req);
	free(work_req);
}

void DestroyEnvironment(const Nan::FunctionCallbackInfo<Value> &args)
{
	if (args.This()->InternalFieldCount() != 1)
	{
		return Nan::ThrowError("Environment not available for instance");
	}

	genaro_env_t *env = (genaro_env_t *)args.This()->GetAlignedPointerFromInternalField(0);
	if (!env)
	{
		return Nan::ThrowError("Environment is not initialized");
	}

	if (genaro_destroy_env(env))
	{
		Nan::ThrowError("Unable to destroy environment");
	}
	args.This()->SetAlignedPointerInInternalField(0, NULL);
}

void FreeEnvironmentCallback(const Nan::WeakCallbackInfo<free_env_proxy> &data)
{
	free_env_proxy *proxy = data.GetParameter();
	Local<Object> obj = Nan::New<Object>(proxy->persistent);
	genaro_env_t *env = (genaro_env_t *)obj->GetAlignedPointerFromInternalField(0);

	if (env && genaro_destroy_env(env))
	{
		Nan::ThrowError("Unable to destroy environment");
	}
	delete proxy;
}

void Environment(const v8::FunctionCallbackInfo<Value> &args)
{
	Nan::EscapableHandleScope scope;
	if (args.Length() == 0)
	{
		return Nan::ThrowError("First argument is expected");
	}

	v8::Local<v8::Object> options = args[0].As<v8::Object>();

	v8::Local<v8::String> bridgeUrl = options->Get(Nan::New("bridgeUrl").ToLocalChecked()).As<v8::String>();
	v8::Local<v8::String> key_file = options->Get(Nan::New("keyFile").ToLocalChecked()).As<v8::String>();
	v8::Local<v8::String> passphrase = options->Get(Nan::New("passphrase").ToLocalChecked()).As<v8::String>();
	Nan::MaybeLocal<Value> user_agent = options->Get(Nan::New("userAgent").ToLocalChecked());
	Nan::MaybeLocal<Value> logLevel = options->Get(Nan::New("logLevel").ToLocalChecked());

	v8::Local<v8::FunctionTemplate> constructor = Nan::New<v8::FunctionTemplate>();
	constructor->SetClassName(Nan::New("Environment").ToLocalChecked());
	constructor->InstanceTemplate()->SetInternalFieldCount(1);

	Nan::SetPrototypeMethod(constructor, "getInfo", GetInfo);
	Nan::SetPrototypeMethod(constructor, "getBuckets", GetBuckets);
	Nan::SetPrototypeMethod(constructor, "createBucket", CreateBucket);
	Nan::SetPrototypeMethod(constructor, "deleteBucket", DeleteBucket);
	Nan::SetPrototypeMethod(constructor, "renameBucket", RenameBucket);
	Nan::SetPrototypeMethod(constructor, "listFiles", ListFiles);
	Nan::SetPrototypeMethod(constructor, "generateEncryptionInfo", GenerateEncryptionInfo);
	Nan::SetPrototypeMethod(constructor, "storeFile", StoreFile);
	Nan::SetPrototypeMethod(constructor, "storeFileCancel", StoreFileCancel);
	Nan::SetPrototypeMethod(constructor, "resolveFile", ResolveFile);
	Nan::SetPrototypeMethod(constructor, "resolveFileCancel", ResolveFileCancel);
	Nan::SetPrototypeMethod(constructor, "deleteFile", DeleteFile);
	Nan::SetPrototypeMethod(constructor, "encryptMeta", EncryptMeta);
	Nan::SetPrototypeMethod(constructor, "encryptMetaToFile", EncryptMetaToFile);
	Nan::SetPrototypeMethod(constructor, "decryptMeta", DecryptMeta);
	Nan::SetPrototypeMethod(constructor, "decryptMetaFromFile", DecryptMetaFromFile);
	Nan::SetPrototypeMethod(constructor, "decryptFile", DecryptFile);
	Nan::SetPrototypeMethod(constructor, "destroy", DestroyEnvironment);

	Nan::MaybeLocal<v8::Object> maybeInstance;
	v8::Local<v8::Object> instance;

	v8::Local<v8::Value> *argv = 0;
	maybeInstance = Nan::NewInstance(constructor->GetFunction(), 0, argv);

	if (maybeInstance.IsEmpty())
	{
		return Nan::ThrowError("Could not create new Genaro instance");
	}
	else
	{
		instance = maybeInstance.ToLocalChecked();
	}

	// Bridge URL handling
	Nan::Utf8String _bridgeUrl(bridgeUrl);
	const char *url = *_bridgeUrl;
	char proto[6];
	char host[100];
	int port = 0;
	sscanf(url, "%5[^://]://%99[^:/]:%99d", proto, host, &port);
	if (port == 0)
	{
		if (strcmp(proto, "http") == 0)
		{
			port = 80;
		}
		else
		{
			port = 443;
		}
	}

	// V8 types to C types
	Nan::Utf8String _keyFileObj(key_file);
	const char *_key_file = *_keyFileObj;
	Nan::Utf8String _passphraseObj(passphrase);
	const char *_passphrase = *_passphraseObj;

	// Setup option structs
	genaro_bridge_options_t bridge_options = {};
	bridge_options.proto = proto;
	bridge_options.host = host;
	bridge_options.port = port;

	json_object *key_json_obj = json_tokener_parse(_key_file);
	key_result_t *key_result = genaro_parse_key_file(key_json_obj, _passphrase);
	if (key_result == NULL)
	{
		json_object_put(key_json_obj);
		return Nan::ThrowError("Key file and passphrase mismatch.");
	}
	genaro_encrypt_options_t encrypt_options;
	genaro_key_result_to_encrypt_options(key_result, &encrypt_options);
	json_object_put(key_json_obj);

	genaro_http_options_t http_options = {};
	if (!user_agent.ToLocalChecked()->IsNullOrUndefined())
	{
		Nan::Utf8String str(user_agent.ToLocalChecked());
		http_options.user_agent = strdup(*str);
	}
	else
	{
		http_options.user_agent = "genaro-test";
	}
	http_options.low_speed_limit = GENARO_LOW_SPEED_LIMIT;
	http_options.low_speed_time = GENARO_LOW_SPEED_TIME;
	http_options.timeout = GENARO_HTTP_TIMEOUT;
	http_options.cainfo_path = NULL;

	static genaro_log_options_t log_options = {};
	log_options.logger = JsonLogger;
	log_options.level = 0;
	if (!logLevel.ToLocalChecked()->IsNullOrUndefined())
	{
		log_options.level = To<int>(logLevel.ToLocalChecked()).FromJust();
	}

	// Initialize environment
	genaro_env_t *env = genaro_init_env(&bridge_options,
		&encrypt_options,
		&http_options,
		&log_options,
		true);

	free(encrypt_options.priv_key);

	if (!env)
	{
		Nan::ThrowError("Environment is not initialized");
	}

	// Make sure that the loop is the default loop
	env->loop = uv_default_loop();

	free_env_proxy *proxy = new free_env_proxy();

	// Pass along the environment so it can be accessed by methods
	instance->SetAlignedPointerInInternalField(0, env);

	Nan::Persistent<v8::Object> persistent(instance);

	// Setup reference
	proxy->persistent.Reset(persistent);

	// There is no guarantee that the free callback will be called
	persistent.SetWeak(proxy, FreeEnvironmentCallback, v8::WeakCallbackType::kParameter);
	persistent.MarkIndependent();
	Nan::AdjustExternalMemory(sizeof(genaro_env_t));

	args.GetReturnValue().Set(persistent);
}

void init(Handle<Object> exports)
{
	NODE_SET_METHOD(exports, "Environment", Environment);
	NODE_SET_METHOD(exports, "utilTimestamp", Timestamp);
}

NODE_MODULE(genaro, init);
