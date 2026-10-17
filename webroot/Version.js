log = console.log;

function getVersion()
{
  let versionPromise = RequestUtils.get("/api/version");
  versionPromise.then((data) => {
    let versionSection = document.getElementById("versionSection");
    let text = `Version ${data.version}`;
    log("Halomux", text);
    versionSection.textContent = text;
  });
  versionPromise.catch((error) => {log(error);});
}

getVersion();
