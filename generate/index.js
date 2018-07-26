var generateJson = require("./scripts/generateJson");
var generateNativeCode = require("./scripts/generateNativeCode");
var generateMissingTests = require("./scripts/generateMissingTests");
var submoduleStatus = require("../lifecycleScripts/submodules/getStatus");

module.exports = function generate() {
  console.log("[nodegit] Generating native code");

  return submoduleStatus()
    .then(function(statuses) {
      var dirtySubmodules = statuses
        .filter(function(status) {
          // For explanaition why libssh2 is special see comment in
          // lifecycleScrpts/submodules/index.js
          return status.onNewCommit
            || status.needsInitialization
            || (status.workDirDirty && status.name != "vendor/libssh2")
        });

      if (dirtySubmodules.length) {
        console.warn("[nodegit] WARNING - Some submodules are out-of-sync");
        dirtySubmodules.forEach(function(submodule) {
          console.warn("[nodegit]\t" + submodule.name);
        });
      }
    })
    .then(function() {
      generateJson();
      generateNativeCode();
      generateMissingTests();
    })
    .catch(function(e) {
      console.error("[nodegit] ERROR - Could not generate native code");
      console.error(e);
    });
}

if (require.main === module) {
  module.exports();
}
