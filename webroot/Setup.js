log = console.log;


/**
 * Initial setup: finds the bridge, pairs with it and checks for an entertainment area
 */
class SetupWizard
{
  constructor()
  {
    this.steps = [...document.querySelectorAll(".step")];
    // Steps counted in the progress, the others (aborted) aren't
    this.progressSteps = ["welcome", "bridge", "pairing", "area", "ready"];

    this.progressLabelNode = document.getElementById("progressLabel");
    this.progressBarNode = document.getElementById("progressBar");

    for(let button of document.querySelectorAll("[data-action]")){
      button.addEventListener("click", () => {
        switch(button.dataset.action){
          case "next": this._showStep(this._nextStep()); break;
          case "back": this._showStep(this._previousStep()); break;
          case "abort": this._abort(); break;
        }
      });
    }

    // Welcome: the notice must be accepted before going on
    this.noticeAcceptedNode = document.getElementById("noticeAccepted");
    this.getStartedButton = document.getElementById("getStartedButton");
    this.noticeAcceptedNode.addEventListener("change", (event) => {this.getStartedButton.disabled = !event.target.checked;});
    this.getStartedButton.addEventListener("click", () => {this._acceptNotice();});

    // Bridge
    this.bridgeAddressNode = document.getElementById("bridgeAddress");
    this.bridgeAddressNode.addEventListener("keydown", (event) => {if(event.key == "Enter"){this._validateAddress();}});
    this.bridgeCandidatesNode = document.getElementById("bridgeCandidates");
    this.bridgeSearchNode = document.getElementById("bridgeSearch");
    document.getElementById("validateAddressButton").addEventListener("click", () => {this._validateAddress();});
    document.getElementById("searchAgainButton").addEventListener("click", () => {this._searchBridges();});

    // Pairing
    this.pairingStatusNode = document.getElementById("pairingStatus");
    this.retryPairingButton = document.getElementById("retryPairingButton");
    this.retryPairingButton.addEventListener("click", () => {this._startPairing();});
    document.getElementById("validateCredentialsButton").addEventListener("click", () => {this._validateCredentials();});

    // Entertainment area
    this.areaListNode = document.getElementById("areaList");
    this.noAreaSectionNode = document.getElementById("noAreaSection");
    this.areaContinueButton = document.getElementById("areaContinueButton");
    document.getElementById("checkAreasButton").addEventListener("click", () => {this._checkAreas();});

    // Ready
    this.startingStatusNode = document.getElementById("startingStatus");
    this.finishButton = document.getElementById("finishButton");
    this.finishButton.addEventListener("click", () => {this._finish();});

    let languageSelectNode = document.getElementById("languageSelect");
    languageSelectNode.value = I18n.setting;
    languageSelectNode.addEventListener("change", (event) => {I18n.setLanguage(event.target.value);});

    RequestUtils.get("/api/configFilePath")
    .then((data) => {document.getElementById("configFilePath").textContent = data.configFilePath;})
    .catch((error) => {log(error);});

    this._showStep("welcome");
  }


  _nextStep()
  {
    return this.progressSteps[this.progressSteps.indexOf(this.currentStep) + 1];
  }


  _previousStep()
  {
    return this.progressSteps[Math.max(0, this.progressSteps.indexOf(this.currentStep) - 1)];
  }


  _showStep(name)
  {
    this._stopTimers();
    this.currentStep = name;

    for(let step of this.steps){
      step.classList.toggle("current", step.dataset.step == name);
    }

    let index = this.progressSteps.indexOf(name);
    this.progressLabelNode.textContent = index >= 0 ? t("Step {step} of {count}", {step : index + 1, count : this.progressSteps.length}) : "";
    this.progressBarNode.style.width = index >= 0 ? `${(index + 1) / this.progressSteps.length * 100}%` : "0";

    for(let error of document.querySelectorAll(".errorMessage:not(.visible)")){
      error.textContent = "";
    }

    switch(name){
      case "bridge": this._searchBridges(); break;
      case "pairing": this._startPairing(); break;
      case "area": this._checkAreas(); break;
    }
  }


  _stopTimers()
  {
    clearTimeout(this.pairingTimer);
    clearTimeout(this.areaTimer);
    this.pairingTimer = null;
    this.areaTimer = null;
  }


  // Welcome

  // Records the acceptance with its time in the configuration, then goes on
  _acceptNotice()
  {
    if(!this.noticeAcceptedNode.checked){
      return;
    }

    RequestUtils.put("/api/acceptNotice", JSON.stringify(null))
    .then(() => {this._showStep("bridge");})
    .catch((error) => {log(error);});
  }


  // Bridge

  _searchBridges()
  {
    this.bridgeSearchNode.style.display = "";
    this.bridgeCandidatesNode.replaceChildren();

    RequestUtils.get("/api/autodetectBridge")
    .then((data) => {
      this.bridgeSearchNode.style.display = "none";

      if(!data.succeeded){
        this._showError("bridgeAddressError", t("The bridge search needs internet access. Enter the address instead."));
        return;
      }

      if(data.bridges.length == 0){
        this._showError("bridgeAddressError", t("No bridge was found. Enter its address instead."));
        return;
      }

      for(let bridge of data.bridges){
        let choice = this._choice(bridge.internalipaddress, t("Hue bridge {id}", {id : (bridge.id || "").slice(-6).toUpperCase()}), () => {
          this.bridgeAddressNode.value = bridge.internalipaddress;
        });
        this.bridgeCandidatesNode.appendChild(choice);
      }

      // A single bridge is preselected
      if(data.bridges.length == 1 && !this.bridgeAddressNode.value){
        this.bridgeCandidatesNode.firstChild.click();
      }
    })
    .catch((error) => {
      log(error);
      this.bridgeSearchNode.style.display = "none";
    });
  }


  _choice(title, detail, onSelect)
  {
    let choice = document.createElement("button");
    choice.type = "button";
    choice.className = "choice";

    let titleNode = document.createElement("strong");
    titleNode.textContent = title;
    let detailNode = document.createElement("span");
    detailNode.className = "hint";
    detailNode.textContent = detail;

    choice.replaceChildren(titleNode, detailNode);
    choice.addEventListener("click", () => {
      for(let other of choice.parentNode.children){
        other.classList.toggle("selected", other == choice);
      }
      onSelect();
    });

    return choice;
  }


  _validateAddress()
  {
    let address = this.bridgeAddressNode.value.trim();
    if(!address){
      this._showError("bridgeAddressError", t("Pick a bridge or enter its address."));
      return;
    }

    RequestUtils.put("/api/validateBridgeAddress", JSON.stringify({bridgeAddress : address}))
    .then((data) => {
      if(data.succeeded){
        this._showStep("pairing");
      }
      else{
        this._showError("bridgeAddressError", t("No Hue bridge answers at {address}.", {address : address}));
      }
    })
    .catch((error) => {log(error);});
  }


  // Pairing

  _startPairing()
  {
    this.pairingDeadline = Date.now() + SetupWizard.PairingTimeout;
    this.retryPairingButton.style.display = "none";
    this._showError("pairingError", "");
    this._tryPairing();
  }


  // Registering fails until the button was pressed: try again every few seconds
  _tryPairing()
  {
    if(this.currentStep != "pairing"){
      return;
    }

    let remaining = Math.ceil((this.pairingDeadline - Date.now()) / 1000);
    if(remaining <= 0){
      this.pairingStatusNode.textContent = "";
      this._showError("pairingError", t("The button wasn't pressed in time. Press it, then try again."));
      this.retryPairingButton.style.display = "";
      return;
    }

    this.pairingStatusNode.textContent = t("Waiting for the button… {seconds} s", {seconds : remaining});

    RequestUtils.put("/api/registerNewUser", JSON.stringify(null))
    .then((data) => {
      if(data.succeeded){
        this._paired();
        return;
      }

      if(data.error && !String(data.error).includes("link button")){
        // Anything else than the button not being pressed yet
        this._showError("pairingError", t("The bridge refused the pairing: {error}", {error : data.error}));
      }

      this.pairingTimer = setTimeout(() => {this._tryPairing();}, SetupWizard.PairingInterval);
    })
    .catch((error) => {
      log(error);
      this.pairingTimer = setTimeout(() => {this._tryPairing();}, SetupWizard.PairingInterval);
    });
  }


  _paired()
  {
    this._stopTimers();
    this.pairingStatusNode.textContent = t("Paired with the bridge ✓");
    this.pairingStatusNode.classList.add("success");
    setTimeout(() => {this._showStep("area");}, 1200);
  }


  _validateCredentials()
  {
    let username = document.getElementById("username").value.trim();
    let clientkey = document.getElementById("clientkey").value.trim();

    if(!username || !clientkey){
      this._showError("credentialsError", t("Enter both the username and the client key."));
      return;
    }

    RequestUtils.put("/api/validateCredentials", JSON.stringify({username : username, clientkey : clientkey}))
    .then((data) => {
      if(data.succeeded){
        this._paired();
      }
      else{
        this._showError("credentialsError", t("The bridge doesn't accept these credentials."));
      }
    })
    .catch((error) => {log(error);});
  }


  // Entertainment area

  _checkAreas()
  {
    clearTimeout(this.areaTimer);

    RequestUtils.get("/api/entertainmentAreas")
    .then((data) => {
      if(this.currentStep != "area"){
        return;
      }

      this.areaListNode.replaceChildren();
      for(let area of data.areas){
        let choice = this._choice(area.name, t("{count} lamps", {count : area.lamps}), () => {});
        choice.disabled = true;
        choice.classList.add("selected");
        this.areaListNode.appendChild(choice);
      }

      let found = data.areas.length > 0;
      this.noAreaSectionNode.style.display = found ? "none" : "";
      this.areaContinueButton.disabled = !found;

      if(!found){
        this.areaTimer = setTimeout(() => {this._checkAreas();}, SetupWizard.AreaCheckInterval);
      }
    })
    .catch((error) => {log(error);});
  }


  // Ready

  _finish()
  {
    this.finishButton.disabled = true;
    this.startingStatusNode.textContent = t("Starting the light manager…");

    RequestUtils.post("/api/finishSetup", JSON.stringify(null))
    .then(() => {this._waitForLightManager();})
    .catch((error) => {
      log(error);
      this._waitForLightManager();
    });
  }


  // The light manager replaces this page once the screen was chosen
  _waitForLightManager()
  {
    RequestUtils.get("/api/webUIStatus")
    .then(() => {window.location.href = "/";})
    .catch(() => {setTimeout(() => {this._waitForLightManager();}, 1000);});
  }


  _abort()
  {
    RequestUtils.post("/api/abort", JSON.stringify(null))
    .catch((error) => {log(error);})
    .finally(() => {this._showStep("aborted");});
  }


  _showError(id, message)
  {
    document.getElementById(id).textContent = message;
  }
}


// Pairing attempts while waiting for the button [ms]
SetupWizard.PairingInterval = 2000;
SetupWizard.PairingTimeout = 90000;
// Entertainment area checks while there is none [ms]
SetupWizard.AreaCheckInterval = 5000;


let setupWizard = null;

I18n.init().then(() => {
  setupWizard = new SetupWizard();
});
