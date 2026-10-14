log = console.log;

function getVersion()
{
  let versionPromise = RequestUtils.get("/api/version");
  versionPromise.then((data) => {
    let versionSection = document.getElementById("versionSection");
    let text = `Version ${data.version}`;
    log("Philip's Hue Desktop Sync", text);
    versionSection.innerHTML = text;
  });
  versionPromise.catch((error) => {log(error);});
}

getVersion();
