var addon = require('./build/Release/addon');

console.log( 'create runtime:', addon.create("runtime") );

console.log( 'create videotestsource:', addon.create("videotestsource") );

console.log( '',addon.invoke("videotestsrc0", "set_runtime", ["pipeline0"] ));

