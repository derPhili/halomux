log = console.log;


class SetupUI
{
  constructor()
  {
    this.setupSection = document.getElementById("setupSection");
    this.stepNodes = document.getElementsByClassName("step");
    this.errorMessages = document.getElementsByClassName("errorMessage");

    this.setConfigFilePath();

    this.start();
  }


  setConfigFilePath()
  {
    RequestUtils.get("/api/configFilePath")
    .then((jsonData) => {
      let configFileLink = document.getElementById("configFilePath");
      configFileLink.href = `file://${jsonData.configFilePath}`;
      configFileLink.innerHTML = jsonData.configFilePath;
    })
    .catch((error) => {
      log(error);
    });
  }


  start()
  {
    this.stepId = 0;
    this._loadStep(this.stepId);
  }


  incStep()
  {
    this._loadStep(this.stepId + 1);
  }


  abort()
  {
    this._exit();
  }


  // Bridge address step
  autodetectBridge()
  {
    let autodetectBridgePromise = RequestUtils.get("/api/autodetectBridge");
    autodetectBridgePromise.then((data) => {this.autodetectBridgeCallback(data)});
    autodetectBridgePromise.catch((error) => {log(error);});
  }


  autodetectBridgeCallback(data)
  {
    let errorMessageNode = document.getElementById("bridgeAddressErrorMessage");

    if(data.succeeded){
      let fieldNode = document.getElementById("bridgeAddress");
      let multipleBridgeNode = document.getElementById("multipleBridgeSection");

      if(data.bridges.length == 0){
        errorMessageNode.style.visibility = "visible";
        errorMessageNode.innerHTML = "Bridge could not be found";
      }
      else{
        fieldNode.value = data.bridges[0].internalipaddress;

        if(data.bridges.length > 1){
          multipleBridgeNode.style.display = "block";

          let bridgeCandidatesSelectNode = document.getElementById("bridgeCandidates");
          bridgeCandidatesSelectNode.addEventListener("change", (event) => {fieldNode.value = (event.target.value);});

          for(let bridge of data.bridges){
            let newOption = bridgeCandidatesSelectNode.appendChild(document.createElement("option"));
            newOption.innerHTML = `${bridge.internalipaddress}`;
          }
        }
        else{
          multipleBridgeNode.style.display = "none";
        }
      }
    }
    else{
      errorMessageNode.style.visibility = "visible";
      errorMessageNode.innerHTML = data.error;
    }
  }


  validateAddress()
  {
    let bridgeAddress = document.getElementById("bridgeAddress").value;

    if(bridgeAddress != ""){
      let validateBridgeAddressPromise = RequestUtils.put("/api/validateBridgeAddress", JSON.stringify({bridgeAddress : bridgeAddress}));
      validateBridgeAddressPromise.then((data) => {this.validateBridgeAddressCallback(data);});
      validateBridgeAddressPromise.catch((error) => {log(error);})
    }
    else{
      let errorMessageNode = document.getElementById("bridgeAddressErrorMessage");
      errorMessageNode.style.visibility = "visible";
      errorMessageNode.innerHTML = "Please provide a valid address";
    }
  }


  validateBridgeAddressCallback(data)
  {
    if(data.succeeded){
      this.incStep();
    }
    else{
      let errorMessageNode = document.getElementById("bridgeAddressErrorMessage");
      errorMessageNode.style.visibility = "visible";
      errorMessageNode.innerHTML = "Could not resolve Hue bridge";
    }
  }


  // Credentials validation step
  validateCredentials()
  {
    const username = document.getElementById("username").value;
    const clientkey = document.getElementById("clientkey").value;
    let errorMessageNode = document.getElementById("credentialsErrorMessage");

    if(username == "" || clientkey == ""){
      errorMessageNode.style.visibility = "visible";
      errorMessageNode.innerHTML = "Please provide valid credentials (or quickly generate new ones)";
      return;
    }

    const regex = new RegExp(/^[A-F0-9]{32}$/);
    if(!regex.test(clientkey)){
      errorMessageNode.style.visibility = "visible";
      errorMessageNode.innerHTML = "The clientkey doesn't match the format of 32 hexadecimal characters";
      return;
    }

    let validateCredentialsPromise = RequestUtils.put("/api/validateCredentials", JSON.stringify({username : username, clientkey : clientkey}));
    validateCredentialsPromise.then((data) => {this.validateCredentialsCallback(data);});
    validateCredentialsPromise.catch((error) => {log(error);});
  }


  validateCredentialsCallback(data)
  {

    if(data.succeeded){
      this._finish();
    }
    else{
      let errorMessageNode = document.getElementById("credentialsErrorMessage");
      errorMessageNode.style.visibility = "visible";
      errorMessageNode.innerHTML = "The provided username was denied";
    }
  }


  // Credentials generation step
  registerNewUser()
  {
    let registerNewUserPromise = RequestUtils.put("/api/registerNewUser", JSON.stringify(null));
    registerNewUserPromise.then((data) => {this.registerNewUserCallback(data);});
    registerNewUserPromise.catch((error) => {log(error);});
  }


  registerNewUserCallback(data)
  {
    if(data.succeeded){
      this._finish();
    }
    else{
      let errorMessageNode = document.getElementById("newUserErrorMessage");
      errorMessageNode.style.visibility = "visible";
      errorMessageNode.innerHTML = "Please press the central button on the bridge and try again";
    }
  }


  // Private methods
  _loadStep(stepId)
  {
    this.stepId = stepId;
    let i = 0;
    for(let stepNode of this.stepNodes){
      // "" restores the stylesheet layout (flex)
      let display = (i++ == this.stepId) ? "" : "none";
      stepNode.style.display = display;
    }

    for(let errorMessageNode of this.errorMessages){
      errorMessageNode.style.visibility = "hidden";
      errorMessageNode.innerHTML = "";
    }
  }

  _finish()
  {
    let finishSetupPromise = RequestUtils.post("/api/finishSetup", JSON.stringify(null));
    finishSetupPromise.then((data) => {log("Finished");});
    finishSetupPromise.catch((error) => {log(error);})

    for(let stepNode of this.stepNodes){
      stepNode.style.display = "none";
    }

    document.getElementById("finishSection").style.display = "flex";

    this.pingFunctionInterval = setInterval(
      this._ping
      , 500);
  }


  _exit()
  {
    let abortPromise = RequestUtils.post("/api/abort", JSON.stringify(null));
    abortPromise.then((data) => {});
    abortPromise.catch((error) => {log(error);});

    for(let stepNode of this.stepNodes){
      stepNode.style.display = "none";
    }

    document.getElementById("abortSection").style.display = "flex";
  }


  _ping()
  {
    let webUIStatusPromise = RequestUtils.get("/api/webUIStatus");
    webUIStatusPromise.then((data) => {
      document.getElementById("refreshSection").style.display = "block";
      clearInterval(setupUI.pingFunctionInterval);
    });
    webUIStatusPromise.catch((error) => {log(error);});
  }
}

let setupUI = new SetupUI();
