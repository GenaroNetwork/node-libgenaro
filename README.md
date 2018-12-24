# node-libgenaro

Node.js library for encrypted file transfer on the GenaroNetwork network via bindings to [libgenaro](https://github.com/GenaroNetwork/libgenaro).

## Example Usage

Install via npm:

```bash
npm install libgenaro
```

First setup the storj environment with authentication and encryption options:

```js
const { Environment } = require('libgenaro');

const libgenaro = new Environment({
    bridgeUrl: 'http://111.111.111.111:8080',
    keyFile: `{
        "address": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",
        "crypto":{"ciphertext":"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",
        "cipherparams":{"iv":"cccccccccccccccccccccccccccccccc"},"cipher":"aes-128-ctr",
        "kdf":"scrypt","kdfparams":{"dklen":32,"salt":"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd","n":262144,"r":8,
        "p":1},"mac":"eeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeeee"},
        "id": "ffffffff-1111-2222-3333-444444444444",
        "version": 3
    }
    `,
    passphrase: '123456',
});
```

Upload a file to a bucket:

```js
const bucketId = '368be0816766b28fd5f43af5';
const filePath = './test-upload.data';

const keyCtr = env.generateEncryptionInfo(bucketId);
const index = keyCtr.index;
const key = keyCtr.key;
const ctr = keyCtr.ctr;

const rsaKey = xxxxxx; // encrypted key with rsa
const rsaCtr = xxxxxx; // encrypted ctr with rsa

const state = libgenaro.storeFile(bucketId, fileOrData, isFilePath, {
  filename: 'test-upload.data',
  progressCallback: function(progress, fileBytes) {
    console.log('Progress:', progress);
  },
  finishedCallback: function(err, fileId, fileBytes, sha256_of_encrypted) {
    if (err) {
      return console.error(err);
    }
    console.log('File complete:', fileId);
  },
  index: index,
  key: key,
  ctr: ctr,
  rsaKey: rsaKey,
  rsaCtr: rsaCtr,
});

```

Download a file from a bucket:

```js
const bucketId = '368be0816766b28fd5f43af5';
const fileId = '998960317b6725a3f8080c2b';
const downloadFilePath = './test-download.data';

const key = xxxxxx; // the file encryption key
const ctr = xxxxxx; // the file encryption ctr

const state = libgenaro.resolveFile(bucketId, fileId, filePath, {
  key: key,
  ctr: ctr,
  overwrite: overwrite,
  decrypt: decrypt,
  progressCallback: function(progress, fileBytes) {
    console.log('progress:', progress);
  },
  finishedCallback: function(err, fileBytes, sha256) {
    if (err) {
      return console.error(err);
    }
    console.log('File download complete');
  }
});
```

Once finished, you should call to zero and free memory holding encryption keys:

```js
libgenaro.destroy();
```

## API

- `Environment(options)` - A constructor for keeping encryption options and other environment settings, see available methods below

Methods available on an instance of `Environment`:

- `getInfo(function(err, result) {})` - Get general API info`
- `getBuckets(function(err, result) {})` - Get list of available buckets
- `deleteBucket(bucketId, function(err, result) {})` - Delete a bucket
- `renameBucket(bucketId, function(err) {})` - Rename a bucket
- `listFiles(bucketId, function(err, result) {})` - List files in a bucket
- `storeFile(bucketId, fileOrData, isFilePath, options)` - Upload a file, return state object
- `storeFileCancel(state)` - Cancel an upload
- `resolveFile(bucketId, fileId, filePath, options)` - Download a file, return state object
- `resolveFileCancel(state)` - Cancel a download
- `deleteFile(bucketId, fileId, function(err, result) {})` - Delete a file from a bucket
- `generateEncryptionInfo(bucketId)` - Generate the key and ctr of AES-256-CTR for file encryption, and also the index related to the key and ctr, return undefined if fail
- `decryptFile(filePath, key, ctr)` - Decrypt the undecrypted file use the key and ctr of AES-256-CTR, return the decrypted data if success, undefined if fail
- `encryptMeta(meta)` - Encrypt the meta use AES-256-GCM combined with HMAC-SHA512, return the encrypted meta if success, undefined if fail
- `encryptMetaToFile(meta, filePath)` - Encrypt the meta use AES-256-GCM combined with HMAC-SHA512 to filePath
- `decryptMeta(encryptedMeta)` - Decrypt the encryptedMeta, return the decrypted meta if success, undefined if fail
- `decryptMetaFromFile(filePath)` - Decrypt the data in filePath, return the decrypted data if success, undefined if fail
- `destroy()` - Zero and free memory of encryption keys and the environment
