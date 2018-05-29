bash 中执行
```bash
npm i libgenaro
```


js 中引用
```js
const { Enviroment } = require "genaro"；
let env = new Enviroment({
  bridgeUrl: 'http://101.132.159.197:8080',
  bridgeUser: '',
  bridgePass: '',
  bridgeApiKey: 'xx8e46597942fdc6216a57d2fccae212258678f4f7',
  bridgeSecretKey: 'xxf18954a1034d3adf0fe16f5a33516846dd98140e',
  encryptionKey: 'abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about',
  logLevel: 4
});
```

创建 bucket
```js
genaro.createBucket("test Bucket", (err, result) => {
    console.log(result);
});
//  result：
//  {
//    decrypted:true,
//    id:"97bb812d9265b9b9d275049f",
//    name:"test Bucket2",
//  }
```

查看 bucket 列表
```js
genaro.getBucket((err, result) => {
    console.log(result);
});
// result 为 bucket 数组
// {
//   created: Date,
//   decrypted:true,
//   id:"6fc8272d415b72d4f114ceb1",
//   name:"test Bucket",
// }

```

删除 bucket
```js
genaro.deleteBucket("97bb812d9265b9b9d275049f", (err) => {
    console.log(err);
});
```

上传文件
```js
let state = genaro.storeFile("6fc8272d415b72d4f114ceb1", "/Users/flowfire/Desktop/test-genaro/test.txt", {
  filename: this.fileName,
  progressCallback: (...params) => { console.log(params); },
  finishedCallback: (...params) => { console.log(params); },
  });
// filename 文件名
// progressCallback 上传进度变化时的回调函数
// finishCallback 上传结束后的回调函数
```

查看文件列表
```js
genaro.listFiles("6fc8272d415b72d4f114ceb1", (err, result) => {
    console.log(result);
// result 为 file 数组
// {
//   created: Date(),
//   filename:"test",
//   id:"c68c2ac4497bf85ed2690589",
//   mimetype:"application/octet-stream",
//   size:10,
// }
});
```

下载文件
```js
let state = genaro.resolveFile("6fc8272d415b72d4f114ceb1", "c68c2ac4497bf85ed2690589", "/Users/flowfire/Desktop/test-genaro/test.txt", {
    overwrite: true,
    progressCallback: (...params) => { console.log(params); },
    finishedCallback: (...params) => { console.log(params); },
});
// overwrite 若文件已存在，是否覆盖
// progressCallback 上传进度变化时的回调函数
// finishCallback 上传结束后的回调函数
```

删除文件
```js
genaro.deleteFile("6fc8272d415b72d4f114ceb1", "c68c2ac4497bf85ed2690589",(err) => {
    console.log(err);
});
```