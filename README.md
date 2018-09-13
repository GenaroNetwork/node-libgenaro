# node-libgenaro

[![Build Status](https://travis-ci.org/GenaroNetwork/node-libgenaro.svg?branch=master)](https://travis-ci.org/GenaroNetwork/node-libgenaro)

Node.js library for encrypted file transfer on the GenaroNetwork network via bindings to [libgenaro](https://github.com/GenaroNetwork/libgenaro).

## Example Usage

Install via npm:
```
npm install libgenaro
```

Please see [`./examples`](/examples) directory for example code usage.

First setup the storj environment with authentication and encryption options:

```js
const { Environment } = require('libgenaro');

const libgenaro = new Environment({
  bridgeUrl: 'http://101.132.159.197:8080',
  bridgeUser: 'user@domain.com',
  bridgePass: 'password',
  bridgeApiKey: 'xx0000000000000000000000000000000000000000',
  bridgeSecretKey: 'xx0000000000000000000000000000000000000000',
  encryptionKey: 'abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon abandon about',
  logLevel: 4
});
// pick one of [bridgeUser + bridgePass] and [bridgeApiKey + bridgeSecretKey]
```

Upload a file to a bucket:
```js
const bucketId = '368be0816766b28fd5f43af5';
const filePath = './test-upload.data';

const state = libgenaro.storeFile(bucketId, filePath, {
  filename: 'test-upload.data',
  progressCallback: function(progress, fileBytes) {
    console.log('progress:', progress);
  },
  finishedCallback: function(err, fileId) {
    if (err) {
      return console.error(err);
    }
    console.log('File complete:', fileId);
  }
});

```

Download a file from a bucket:

```js
const bucketId = '368be0816766b28fd5f43af5';
const fileId = '998960317b6725a3f8080c2b';
const downloadFilePath = './test-download.data';

const state = libgenaro.resolveFile(bucketId, fileId, downloadFilePath, {
  progressCallback: function(progress, fileBytes) {
    console.log('progress:', progress)
  },
  finishedCallback: function(err) {
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

Please see [`./examples`](/examples) directory for further usage.

## API

- `.Environment(options)` - A constructor for keeping encryption options and other environment settings, see available methods below
- `.mnemonicGenerate(bits)` - Will create a new *Encryption Key* string for file encryption/decryption
- `.mnemonicCheck(encryptionKey)` - Will return boolean to verify that an *Encryption Key* hasn't been typed incorrectly by verifying the checksum and format
- `.utilTimestamp()` - Returns current unix timestamp in milliseconds

Methods available on an instance of `Environment`:

- `.getInfo(function(err, result) {})` - Get general API info`
- `.getBuckets(function(err, result) {})` - Get list of available buckets
- `.createBucket(bucketName, function(err, result) {})` - Create a bucket
- `.deleteBucket(bucketId, function(err, result) {})` - Delete a bucket
- `.renameBucket(bucketId, function(err) {})` - Rename a bucket
- `.listFiles(bucketId, function(err, result) {})` - List files in a bucket
- `.storeFile(bucketId, filePath, options)` - Upload a file, return state object
- `.storeFileCancel(state)` - Cancel an upload
- `.resolveFile(bucketId, fileId, decrypt_key, filePath, options)` - Download a file, return state object
- `.resolveFileCancel(state)` - Cancel a download
- `.deleteFile(bucketId, fileId, function(err, result) {})` - Delete a file from a bucket
- `.decryptName(encryptedName) {})` - Decrypt a name that is encrypted
- `.ShareFile(bucketId, fileId, function(err) {})` - Share a file
- `.destroy()` - Zero and free memory of encryption keys and the environment


## License

Copyright (C) 2017 Storj Labs, Inc

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU Lesser General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
