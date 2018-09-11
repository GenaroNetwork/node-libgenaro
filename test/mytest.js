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
    //logLevel: 4
});

// env.createBucket('hello', (err, bucket_value) => {
//     console.log(err);
//     console.log(bucket_value);
// });

env.getBuckets((err, buckets) => {
    console.log(err);
    console.log(buckets);
});

// env.renameBucket('b5e9bd5fd6f571beee9b035f', 'w的 ', (err) => {
//     console.log(err);
// });

// setTimeout(() => {
//     env.renameBucket('b5e9bd5fd6f571beee9b035f', 'eee', (err) => {
//         console.log(err);
//     });
// }, 10000);

// env.listFiles('b5e9bd5fd6f571beee9b035f', (err, files) => {
//     console.log(err);
//     console.log(files);
// });

// env.deleteFile('b5e9bd5fd6f571beee9b035f', 
//      '2c64a5a6324f34737fdd3f74', 
//     () => {});

// env.deleteFile('b5e9bd5fd6f571beee9b035f', 
//      '7460895e6ac0701c204f32b0', 
//     () => {});

// env.storeFile('b5e9bd5fd6f571beee9b035f', 
//     '/Users/dingyi/Genaro/test/depends22_x64.zip', 
//     { 'filename': 'depends22_x64.zip',
//         'progressCallback': (progress, fileBytes) => { },
//         'finishedCallback': (err) => { 
//             console.log(err);
//         }
//     }
// );

// let ret11 = env.storeFile('b5e9bd5fd6f571beee9b035f', 
//     '/Users/dingyi/Downloads/Sourcetree_2.7.6a.zip', 
//     { 'filename': 'Sourcetree_2.7.6a.zip',
//         'progressCallback': (progress, fileBytes) => { /*console.log(progress);*/ },
//         'finishedCallback': (err) => {
//             console.log(err);
//         }
//     }
// );

// let ret11 = env.storeFile('b5e9bd5fd6f571beee9b035f', 
//     '/Users/dingyi/Downloads/OS X 10.9.cdr', 
//     { 'filename': 'OS X 10.9.cdr',
//         'progressCallback': (progress, fileBytes) => { console.log(progress); },
//         'finishedCallback': (err) => {
//             console.log(err);
//         }
//     }
// );

// let ret11 = env.storeFile('b5e9bd5fd6f571beee9b035f', 
//     '/Users/dingyi/Downloads/1g.data', 
//     { 'filename': '1g.data',
//         'progressCallback': (progress, fileBytes) => { /*console.log(progress);*/ },
//         'finishedCallback': (err) => {
//             console.log(err);
//         }
//     }
// );

// setTimeout(() => {
//     if(ret11 !== undefined) {
//         console.log("aaa");
//         env.storeFileCancel(ret11);
//         env.storeFileCancel(ret11);
//     }
// }, 5000);

// setTimeout(() => {
//     let ret22 = env.storeFile('b5e9bd5fd6f571beee9b035f', 
//                 '/Users/dingyi/Downloads/Sourcetree_2.7.6a.zip', 
//                 { 'filename': 'Sourcetree_2.7.6a.zip',
//                     'progressCallback': (progress, fileBytes) => { },
//                     'finishedCallback': (err) => { 
//                         console.log(err);
//                     }
//                 }
// );
// console.log(ret22);
// }, 2000);


// env.deleteFile('b5e9bd5fd6f571beee9b035f', 
//                 '9f86987a36cb91dbc18a21c0', 
//                 (err) => { console.log(err); });

// setTimeout(() => {
//     env.storeFile('b5e9bd5fd6f571beee9b035f', 
//     '/Users/dingyi/Downloads/win_x64.zip', 
//     { 'filename': 'win_x64.zip',
//         'progressCallback': (a, b) => {},
//         'finishedCallback': (err) => {
//             console.log(err);
//             env.deleteFile('b5e9bd5fd6f571beee9b035f', 
//                 '9f86987a36cb91dbc18a21c0', 
//                 (err) => { console.log(err); });
//         }
//     });}, 8000);

// env.storeFile('b5e9bd5fd6f571beee9b035f', 
//     '/Users/dingyi/Downloads/Office2016Mac版激活.dmg', 
//     { 'filename': 'Office2016Mac版激活.dmg',
//         'progressCallback': (a, b) => { console.log(a); },
//         'finishedCallback': (err) => {
//             console.log(err);
//         }
//     }
// );

// env.resolveFile('b5e9bd5fd6f571beee9b035f', 
//     '84f67316ee84cec337cf21fc', 
//     '/Users/dingyi/Genaro/test/download/winsdksetup.exe', 
//     { 'overwrite': true, 
//       'progressCallback': (progress, fileBytes) => { console.log(progress); },
//       'finishedCallback': (err) => { console.log(err); }
//     }
// );

// let ret22 = env.resolveFile('b5e9bd5fd6f571beee9b035f', 
//     '5f105bc662081a03d659077f', 
//     '/Users/dingyi/Genaro/test/download/Office2016Mac版激活.dmg', 
//     { 'overwrite': true, 
//       'progressCallback': (a, b) => { console.log(a); },
//       'finishedCallback': (err) => { console.log(err); }
//     }
// );

// env.resolveFile('b5e9bd5fd6f571beee9b035f', 
//     '5f105bc662081a03d659077f', 
//     '/Users/dingyi/Genaro/test/download/Office2016Mac版激活.dmg', 
//     { 'overwrite': true, 
//       'progressCallback': (a, b) => { console.log(a); },
//       'finishedCallback': (err) => { console.log(err); }
//     }
// );

// env.resolveFile('b5e9bd5fd6f571beee9b035f', 
//     '5f105bc662081a03d659077f', 
//     '/Users/dingyi/Genaro/test/download/Office2016Mac版激活.dmg', 
//     { 'overwrite': true, 
//       'progressCallback': (a, b) => { console.log(a); },
//       'finishedCallback': (err) => { console.log(err); }
//     }
// );

// setTimeout(() => {
//     if(ret22 !== undefined) {
//         env.resolveFileCancel(ret22);
//     }
// }, 5000);

// let download1 = env.resolveFile('b5e9bd5fd6f571beee9b035f', 
//     'b4b31bd628bb43295dfa11af', 
//     '/Users/dingyi/Genaro/test/Options_6.90.111.zip', 
//     { 'overwrite': true, 
//       'progressCallback': (a, b) => { console.log(a); },
//       'finishedCallback': (err) => { console.log(err); }
//     }
// );

// setTimeout(() => {
//     env.resolveFileCancel(download1);
// }, 10000);

// setTimeout(() => {
//     env.resolveFile('b5e9bd5fd6f571beee9b035f', 
//     '7ee38521e7c628b0e700514e', 
//     '/Users/dingyi/Genaro/test/download/depends22_x64.zip', 
//     { 'overwrite': true, 
//       'progressCallback': (a, b) => { console.log(a); },
//       'finishedCallback': (err) => { console.log(err); }
//     }
// );
// }, 5000);

let ret0;
let index0 = 0;
let upload;
upload = () => {
    ret0 = env.storeFile('b5e9bd5fd6f571beee9b035f', 
        '/Users/dingyi/Genaro/test/depends22_x64.zip', 
        { 'filename': 'depends22_x64.zip',
            'progressCallback': (a, b) => {},
            'finishedCallback': (err) => {
                if(err === null)
                    console.log(index0++);
                else
                    console.log(err);
                env.deleteFile('b5e9bd5fd6f571beee9b035f', 
                    '7ee38521e7c628b0e700514e', 
                    deleteFinishCallback);
            }
        }
    );
};
function deleteFinishCallback(err)
{
    console.log(err);
    upload();
}

// upload();


// setTimeout(() => {
//     env.deleteFile('b5e9bd5fd6f571beee9b035f', 
//                     '2c64a5a6324f34737fdd3f74', 
//                     deleteFinishCallback);
// }, 7000);



// env.storeFile('b5e9bd5fd6f571beee9b035f', 
//         '/Users/dingyi/Genaro/test/depends22_x64.zip', 
//         { 'filename': 'depends22_x64.zip',
//             'progressCallback': (a, b) => {},
//             'finishedCallback': (err) => {
//                 console.log(err);
//             }
//         }
//     );

let ret1;
let index1 = 0;
let download;
download = () => {
    ret1 = env.resolveFile('5b8c9da88f21182871068f53', 
    '42f68c1a5a6efb3b915db64b', 
    '/Users/dingyi/Genaro/test/download/zip-3.0-bin.zip', 
    { 'overwrite': true,
        'progressCallback': (a, b) => { console.log(a); },
        'finishedCallback': (err) => {
            if(err === null)
                console.log(index1++);
            else
                console.log(err);
            download();
        }
    });
};

download();

function storeFinishCallback(err, d){
    console.log(err);
    // env.deleteFile('b5e9bd5fd6f571beee9b035f', 
    //             '2c64a5a6324f34737fdd3f74', 
    //             deleteFinishCallback);
}

// env.deleteFile('b5e9bd5fd6f571beee9b035f', 
//                 '2c64a5a6324f34737fdd3f74', 
//                 deleteFinishCallback);

// let index = 0;
// env.storeFile('b5e9bd5fd6f571beee9b035f', 
//               '/Users/dingyi/Downloads/555m.data', 
//               {'filename': '555m.data',
//                   'progressCallback': (a, b) => { index++; console.log(index); },
//               'finishedCallback': (d, e, f) => { console.log(d); }
//               }
// );

// const decryptedName = env.decryptName('pAGyxYkdbiXrC8S0eCF/ZsTW90DGwwNecNqtEdhVQfzUkP5vxKM6DjkPiMQrZXJ5Q5lZEOc=');
// if(decryptedName !== undefined)
//     console.log('The name after decrypted  is:' + decryptedName);
// else
//     console.log(`The name to be decrypted is invalid!`);
