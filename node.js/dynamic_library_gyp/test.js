var addon = require('./build/Release/addon');

console.log( 'create runtime returned:', addon.create("runtime") );

console.log( 'create videotestsource returned:', addon.create("videotestsource") );

console.log( 'set_runtime returned:',addon.invoke("videotestsrc0", "set_runtime", ["pipeline0"] ));

console.log( 'set returned:',addon.set("videotestsrc0", "videotestsrc/pattern", "1" ));


