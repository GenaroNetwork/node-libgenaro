'use strict';

module.exports = require('bindings')('genaro.node');

let Environment = module.exports.Environment;
let env = new Environment({
    bridgeUrl: 'http://118.31.61.119:8080',
    keyFile: `{
        "address": "fbad65391d2d2eafda9b27326d1e81d52a6a3dc8",
        "crypto":{"ciphertext":"e968751f3d60827b6e62e3ff6c024ecc82f33a6c55428be33249c83edba444ca","cipherparams":{"iv":"e80d9ec9ba6241a143c756ec78066ad9"},"cipher":"aes-128-ctr","kdf":"scrypt","kdfparams":{"dklen":32,"salt":"ea7cb2b004db67d3103b3790caced7a96b636762f280b243e794fb5bef8ef74b","n":262144,"r":8,"p":1},"mac":"cdb3789e77be8f2a7ab4d205bf1b54e048ad3f5b080b96e07759de7442e050d2"},
        "id": "e28f31b4-1f43-428b-9b12-ab586638d4b1",
        "version": 3
    }
    `,
    passphrase: 'lgygn_9982',
    logLevel: 4
});

// env.createBucket('hello', (err, bucket_value) => {
//     console.log(err);
//     console.log(bucket_value);
// });

// env.getBuckets((err, buckets) => {
//     console.log(err);
//     console.log(buckets);
// });

// env.renameBucket('44be99bb213cf97c926de606', 'eee', (err) => {
//     console.log(err);
// });

// env.listFiles('44be99bb213cf97c926de606', (err, files) => {
//     console.log(err);
//     console.log(files);
// });

// env.deleteFile('44be99bb213cf97c926de606', 
//      '2c64a5a6324f34737fdd3f74', 
//     () => {});

// env.storeFile('44be99bb213cf97c926de606', 
//     '/Users/dingyi/Genaro/test/depends22_x64.zip', 
//     { 'filename': 'depends22_x64.zip',
//         'progressCallback': (a, b, c) => {},
//         'finishedCallback': (err) => {
//             console.log('aaaaa');
//             if(err === null)
//                 console.log(index++);
//             else
//                 console.log(err);
//             // env.deleteFile('44be99bb213cf97c926de606', 
//             //      '2c64a5a6324f34737fdd3f74', 
//             //     (err) => {console.log(err)});
//         }
//     }
// );

env.resolveFile('44be99bb213cf97c926de606', 
              '2c64a5a6324f34737fdd3f74', 
              '/Users/dingyi/Genaro/test/download/depends22_x64.zip', 
              { 'overwrite': true, 
                'progressCallback': (a, b, c, d) => { console.log(a); },
                'finishedCallback': (err, e, f) => { console.log(err); }
              }
);

let ret;
let func;
let index = 0;
func = () => {
    ret = env.storeFile('44be99bb213cf97c926de606', 
    '/Users/dingyi/Genaro/test/depends22_x64.zip', 
    { 'filename': 'depends22_x64.zip',
        'progressCallback': (a, b, c) => {},
        'finishedCallback': (err) => {
            if(err === null)
                console.log(index++);
            else
                console.log(err);
            env.deleteFile('44be99bb213cf97c926de606', 
               '2c64a5a6324f34737fdd3f74', 
                deleteFinishCallback);
          }
        }
);
};

// func();

function storeFinishCallback(err, d){
    console.log(err);
    // env.deleteFile('44be99bb213cf97c926de606', 
    //             '2c64a5a6324f34737fdd3f74', 
    //             deleteFinishCallback);
}

// env.deleteFile('44be99bb213cf97c926de606', 
//                 '2c64a5a6324f34737fdd3f74', 
//                 deleteFinishCallback);

function deleteFinishCallback(err)
{
    console.log(err);
    func();
    // env.storeFile('44be99bb213cf97c926de606', 
    //             '/Users/dingyi/Genaro/test/depends22_x64.zip', 
    //             { 'filename': 'depends22_x64.zip',
    //                 'progressCallback': (a, b, c) => { },
    //                 'finishedCallback': storeFinishCallback}
    // );
}

// let index = 0;
// env.storeFile('44be99bb213cf97c926de606', 
//               '/Users/dingyi/Downloads/555m.data', 
//               {'filename': '555m.data',
//                   'progressCallback': (a, b, c, d) => { index++; console.log(index); },
//               'finishedCallback': (d, e, f) => { console.log(d); }
//               }
// );
