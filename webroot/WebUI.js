class WebUI
{
  constructor()
  {
    // UV updates waiting to be sent, keyed by channel and corner. Only one request is in flight at a
    // time so dragging a handle doesn't flood the backend and responses can't arrive out of order.
    this.pendingUVs = new Map();
    this.uvRequestInFlight = false;
    // Region move waiting for the corner updates before it
    this.pendingMove = null;

    this.activeChannelsNode = document.getElementById("activeChannelsList");
    this.activeChannelsNode.addEventListener("drop", (event) => {
      this._setChannelActivity(JSON.parse(event.dataTransfer.getData("channelData")).channelId, true);
    });

    this.inactiveChannelsNode = document.getElementById("inactiveChannelsList");
    this.inactiveChannelsNode.addEventListener("drop", (event) => {
      this._setChannelActivity(JSON.parse(event.dataTransfer.getData("channelData")).channelId, false);
    });

    document.addEventListener("dragover", (event) => {event.preventDefault();});

    this.activeChannels = {};

    this.screenWidget = new ScreenWidget(this);
    this.syncEnabled = true;
    this.staticScene = new StaticScenePanel(this);
    
    this.entertainmentConfigurationsNode = document.getElementById("entertainmentConfigurations");
    this.entertainmentConfigurationsSelectNode = document.getElementById("entertainmentConfigurationsSelect");
    this.entertainmentConfigurationsSelectNode.addEventListener("change", (event) => {this._setEntertainmentConfiguration(event.target.value);});

    this.availableSubsamplesNode = document.getElementById("availableSubsampleWidths");
    this.availableSubsamplesNode.addEventListener("change", (event) => {this._setSubsampleWidth(parseInt(event.target.value));});
    this.refreshRateInputNode = document.getElementById("refreshRate");
    this.refreshRateLimitNode = document.getElementById("refreshRateLimit");
    this.refreshRateInputNode.addEventListener("change", (event) => {this._setRefreshRate(event.target.valueAsNumber);});
    this.transitionSmoothingInputNode = document.getElementById("transitionSmoothing");
    this.processingInfoNode = document.getElementById("processingInfo");
    this.statusDotNode = document.getElementById("statusDot");

    // Captured monitors, and choosing them again
    this.monitorInfoNode = document.getElementById("monitorInfo");
    this.reselectMonitorsButton = document.getElementById("reselectMonitorsButton");
    this.reselectMonitorsButton.addEventListener("click", () => {this._reselectMonitors();});
    this.monitorSelection = "idle";

    // Live color dot of each active lamp, and the last color shown, by channel ID
    this.lampSwatches = {};
    this.lampColors = {};
    this.lampColorsPaused = false;
    document.addEventListener("visibilitychange", () => {
      if(!document.hidden && this.lampColorsPaused){
        this.lampColorsPaused = false;
        this._refreshLampColors();
      }
    });
    window.addEventListener("resize", () => {this._redrawScreenWidget();});

    this.audioReactiveNode = document.getElementById("audioReactive");
    this.audioReactiveNode.addEventListener("change", (event) => {this._setAudioReactive(event.target.checked);});

    this.highlightBoostInputNode = document.getElementById("highlightBoost");
    this.highlightBoostLabelNode = document.getElementById("highlightBoostLabel");
    this.highlightBoostInputNode.addEventListener("input", (event) => {
      this.highlightBoostLabelNode.textContent = event.target.value;
      this._highlightPreset(this.boostPresets, event.target.valueAsNumber);
    });

    // Highlight boost strengths, values in percent
    this.boostPresets = [
      {button : document.getElementById("subtleBoostButton"), value : 20},
      {button : document.getElementById("moderateBoostButton"), value : 40},
      {button : document.getElementById("highBoostButton"), value : 60},
      {button : document.getElementById("intenseBoostButton"), value : 85}
    ];

    for(let preset of this.boostPresets){
      preset.button.addEventListener("click", () => {this._setHighlightBoost(preset.value);});
    }
    this.highlightBoostInputNode.addEventListener("change", (event) => {this._setHighlightBoost(event.target.valueAsNumber);});

    this.brightnessInputNode = document.getElementById("brightness");
    this.brightnessLabelNode = document.getElementById("brightnessLabel");
    this.brightnessInputNode.addEventListener("input", (event) => {this.brightnessLabelNode.textContent = event.target.value;});
    this.brightnessInputNode.addEventListener("change", (event) => {this._setBrightness(event.target.valueAsNumber);});
    this.transitionSmoothingLabelNode = document.getElementById("transitionSmoothingLabel");
    this.transitionSmoothingInputNode.addEventListener("input", (event) => {
      this.transitionSmoothingLabelNode.textContent = event.target.value;
      this._highlightSmoothingPreset(event.target.valueAsNumber);
    });
    this.transitionSmoothingInputNode.addEventListener("change", (event) => {this._setTransitionSmoothing(event.target.valueAsNumber);});

    // Quick switches between a direct and a smooth transition smoothing, values in percent
    this.smoothingPresets = [
      {button : document.getElementById("gamingPresetButton"), value : WebUI.GamingSmoothing},
      {button : document.getElementById("moviePresetButton"), value : WebUI.MovieSmoothing}
    ];

    for(let preset of this.smoothingPresets){
      preset.button.addEventListener("click", () => {this._setTransitionSmoothing(preset.value);});
    }

    this.previewButton = document.getElementById("previewButton");
    this.previewButtonTitle = this.previewButton.title;
    this.previewEnd = null;
    this.previewTimer = null;
    this.previewButton.addEventListener("click", () => {this._togglePreview();});

    this.saveProfileButton = document.getElementById("saveProfileButton");
    this.saveProfileButton.addEventListener("click", () => {this._saveProfile();});

    this.stopButton = document.getElementById("stopButton");
    this.stopButton.addEventListener("click", () => {this._askStopConfirmation()});
    
    this.overlay = document.getElementById("overlay");

    this.confirmStopButton = document.getElementById("confirmStopButton");
    this.confirmStopButton.addEventListener("click", () => {this._stop();});
    
    this.cancelStopButton = document.getElementById("cancelStopButton");
    this.cancelStopButton.addEventListener("click", () => {this.overlay.style.display = "none";});

    this.languageSelectNode = document.getElementById("languageSelect");
    this.languageSelectNode.value = I18n.setting;
    this.languageSelectNode.addEventListener("change", (event) => {I18n.setLanguage(event.target.value);});
  }


  initUI()
  {
    this._checkNotice();

    let channelsPromise = RequestUtils.get("/api/channels");
    channelsPromise.then((data) => {this._refreshChannelsLists(data);});
    channelsPromise.catch((error) => {log(error);});

    let displayInfoPromise = RequestUtils.get("/api/displayInfo");
    displayInfoPromise.then((data) => {this._displayInfoCallback(data);});
    displayInfoPromise.catch((error) => {log(error);});

    // Refreshed periodically: the grabber falls back to the CPU at runtime if the GPU path fails,
    // and monitors start or stop being captured as lamps move
    this._refreshProcessingInfo();
    setInterval(() => {this._refreshProcessingInfo(); this._refreshMonitors();}, 5000);

    this._refreshLampColors();
    this.staticScene.init();

    let entertainmentConfigurationsPromise = RequestUtils.get("/api/entertainmentConfigurations");
    entertainmentConfigurationsPromise.then((data) => {this._entertainmentConfigurationsCallback(data);});
    entertainmentConfigurationsPromise.catch((error) => {log(error);});
  }


  notifyUV(uvData)
  {
    let channelId = this.screenWidget.currentChannel.channelId;
    this.pendingUVs.set(`${channelId}:${uvData.type}`, {channelId : channelId, uvData : uvData});

    if(!this.uvRequestInFlight){
      this._sendPendingUV();
    }
  }


  _sendPendingUV()
  {
    let next = this.pendingUVs.entries().next();
    if(next.done){
      // A region moved right after dragging a corner: sent once the corners arrived
      if(this.pendingMove){
        let move = this.pendingMove;
        this.pendingMove = null;
        this.moveChannelRegion(move.channelId, move.monitor, move.uvs);
      }
      return;
    }

    let [key, pending] = next.value;
    this.pendingUVs.delete(key);
    this.uvRequestInFlight = true;

    RequestUtils.put(`/api/setChannelUV/${pending.channelId}`, JSON.stringify(pending.uvData))
    .then((checkedUVs) => {
      if(this.activeChannels[pending.channelId]){
        this.activeChannels[pending.channelId].uvs = checkedUVs;
      }

      // Don't move the handles back while newer positions are still waiting to be sent
      let isCurrentChannel = this.screenWidget.currentChannel && this.screenWidget.currentChannel.channelId == pending.channelId;
      if(isCurrentChannel && this.pendingUVs.size == 0){
        this.screenWidget.uvCallback(checkedUVs);
      }
    })
    .catch((error) => {log(error);})
    .finally(() => {
      this.uvRequestInFlight = false;
      this._sendPendingUV();
    });
  }


  /**
   * Moves a lamp's region, onto another monitor too
   */
  moveChannelRegion(channelId, monitor, uvs)
  {
    // Corner positions still on their way would land after the move, on the wrong monitor
    if(this.uvRequestInFlight || this.pendingUVs.size > 0){
      this.pendingMove = {channelId : channelId, monitor : monitor, uvs : uvs};
      return;
    }

    RequestUtils.put(`/api/setChannelMonitor/${channelId}`, JSON.stringify({monitor : monitor, uvs : uvs}))
    .then((channelData) => {
      let channel = this.activeChannels[channelId];
      if(channel){
        channel.uvs = channelData.uvs;
        channel.monitor = channelData.monitor;
        channel.monitorMissing = channelData.monitorMissing;
        this._fillChannelEntry(channel.node, channel);
        this.lampSwatches[channelId] = channel.node.querySelector(".lampSwatch");
      }

      this._refreshMonitors();
    })
    .catch((error) => {log(error);})
    .finally(() => {
      // The handles were hidden while moving
      if(this.screenWidget.currentChannel){
        this.screenWidget.showWidgets(true);
      }
      this._redrawScreenWidget();
    });
  }


  _reselectMonitors()
  {
    RequestUtils.post("/api/reselectMonitors", "{}")
    .then(() => {this._refreshMonitors();})
    .catch((error) => {log(error);});
  }


  _refreshMonitors()
  {
    RequestUtils.get("/api/displayInfo")
    .then((displayInfo) => {this._monitorsCallback(displayInfo);})
    .catch((error) => {log(error);});
  }


  _monitorsCallback(displayInfo)
  {
    let monitors = displayInfo.monitors;
    let shown = monitors.filter(monitor => monitor.active);
    if(shown.length == 0){
      shown = monitors;
    }

    this.monitorInfoNode.textContent = t("Source: ") + shown.map(monitor => monitor.description).join(" · ");

    let previousSelection = this.monitorSelection;
    this.monitorSelection = displayInfo.monitorSelection;

    let pending = displayInfo.monitorSelection == "pending";
    this.reselectMonitorsButton.disabled = pending;
    this.reselectMonitorsButton.textContent = t(pending ? "Waiting for the screen selection…" : "Monitors…");

    if(pending){
      // Until the dialog is answered
      setTimeout(() => {this._refreshMonitors();}, 2000);
    }
    else if(previousSelection == "pending" && displayInfo.monitorSelection == "cancelled"){
      this.reselectMonitorsButton.textContent = t("Selection cancelled, nothing changed");
      setTimeout(() => {this.reselectMonitorsButton.textContent = t("Monitors…");}, 4000);
    }

    // Other monitors captured, from this page or another one: the lamps' monitor indices changed
    let layout = JSON.stringify(monitors.map(monitor => [monitor.name, monitor.x, monitor.y, monitor.width, monitor.height, monitor.pixelWidth, monitor.pixelHeight]));
    if(this.monitorsLayout !== undefined && layout != this.monitorsLayout){
      this.screenWidget.currentChannel = null;
      this.screenWidget.showWidgets(false);
      RequestUtils.get("/api/channels")
      .then((channels) => {this._refreshChannelsLists(channels);})
      .catch((error) => {log(error);});
    }
    this.monitorsLayout = layout;

    let monitorsChanged = JSON.stringify(monitors) != JSON.stringify(this.screenWidget.monitors);
    if(monitorsChanged){
      this.screenWidget.setDimensions(displayInfo.x, displayInfo.y, displayInfo.subsampleWidth, monitors);
      this._redrawScreenWidget();
    }
  }


  updateGammaFactor(gammaFactor)
  {
    if(!this.screenWidget.currentChannel){
      return;
    }

    if(gammaFactor == this.screenWidget.currentChannel.gammaFactor){
      return;
    }

    this.screenWidget.currentChannel.gammaFactor = gammaFactor;

    let promise = RequestUtils.put(`/api/setChannelGammaFactor/${this.screenWidget.currentChannel.channelId}`, JSON.stringify({gammaFactor : gammaFactor}));
    promise.then((gammaFactorData) => {
      let gammaFactor = gammaFactorData.gammaFactor;
      this.screenWidget.currentChannel.gammaFactor = Utils.truncate(gammaFactor, 2);
    });
    promise.catch((error) => {log(error);});
  }


  _setChannelActivity(channelId, active)
  {
    let promise = RequestUtils.post(`/api/setChannelActivity/${channelId}`, JSON.stringify({active : active}));
    promise.then((data) => {
      this._refreshChannelsLists(data.channels);
      
      if("newActiveChannelId" in data){
        this._manageChannel(data.newActiveChannelId);
      }
      else{
        this.screenWidget.showWidgets(false);
      }
    });
    promise.catch((error) => {log(error);});
  }


  _refreshChannelsLists(channels)
  {
    const activeChannels = channels.filter(channel => channel.active);
    const inactiveChannels = channels.filter(channel => !channel.active);

    this.activeChannels = {};
    this._refreshActiveChannels(activeChannels);
    this._refreshInactiveChannels(inactiveChannels);

    if(channels.length == 0){
      this.screenWidget.setLegend(ScreenWidget.Legends.noChannel);
    }
    else if(Object.keys(this.activeChannels).length == 0){
      this.screenWidget.setLegend(ScreenWidget.Legends.pleaseDrag);
      this.screenWidget.showWidgets(false);
    }
    else{
      this.screenWidget.setLegend(ScreenWidget.Legends.pleaseSelect);
    }
    
    this.screenWidget.showPreview();
    this.staticScene.refreshLamps();
  }


  onSyncChanged(syncEnabled)
  {
    this.syncEnabled = syncEnabled;
    this._refreshProcessingInfo();
  }


  _refreshActiveChannels(channels)
  {
    this.activeChannelsNode.innerHTML = "";
    this.lampSwatches = {};
    this.lampColors = {};
    for(let channelData of channels){
      let newChannelEntryNode = document.createElement("p");
      newChannelEntryNode.draggable = true;
      newChannelEntryNode.selected = false;

      let newChannel = new Channel(channelData, newChannelEntryNode)
      newChannelEntryNode.addEventListener("dragstart", (event) => {
        event.dataTransfer.setData("channelData", JSON.stringify(newChannel));
      });

      newChannelEntryNode.addEventListener("click", (event) => {
        this._manageChannel(channelData.channelId);
      });

      this._fillChannelEntry(newChannelEntryNode, newChannel);
      this.lampSwatches[newChannel.channelId] = newChannelEntryNode.querySelector(".lampSwatch");

      this.activeChannelsNode.appendChild(newChannelEntryNode);
      this.activeChannels[newChannel.channelId] = newChannel;
    }
  }


  _fillChannelEntry(entryNode, channel)
  {
    let swatch = document.createElement("span");
    swatch.className = "lampSwatch";

    let name = document.createElement("span");
    name.textContent = channel.formatMembers();

    let id = document.createElement("span");
    id.className = "hint";
    id.style.marginLeft = "auto";
    id.textContent = `#${channel.channelId}`;

    entryNode.replaceChildren(swatch, name, id);

    entryNode.title = channel.node
      ? t("{lamp}: the dot shows its current color. Click to edit its screen region, drag to Available to stop driving it", {lamp : channel.formatMembers()})
      : t("{lamp}: not driven. Drag to Active to have it follow the screen", {lamp : channel.formatMembers()});

    if(channel.node && channel.monitorMissing){
      let missing = document.createElement("span");
      missing.className = "hint monitorMissing";
      missing.textContent = t("Monitor not captured – lamp is off");
      entryNode.insertBefore(missing, id);
      entryNode.title = t("The monitor this lamp followed isn't captured. Choose it again with \"Monitors…\", or drag the region onto a captured monitor.");
    }
  }


  _setItemSelected(channelNode, selected)
  {
    channelNode.selected = selected;

    if(channelNode.selected){
      channelNode.classList.add("selected");
    }
    else{
      channelNode.classList.remove("selected");
    }
  }


  _toggleClicked(channelNode)
  {
    this._setItemSelected(channelNode, !channelNode.selected);
  }


  _refreshInactiveChannels(channels)
  {
    this.inactiveChannelsNode.innerHTML = "";

    for(let channelData of channels){
      let newChannelEntryNode = document.createElement("p");
      newChannelEntryNode.draggable = true;

      let newChannel = new Channel(channelData)
      newChannelEntryNode.addEventListener("dragstart", (event) => {
        event.dataTransfer.setData("channelData", JSON.stringify(newChannel));
      });

      this._fillChannelEntry(newChannelEntryNode, newChannel);

      this.inactiveChannelsNode.appendChild(newChannelEntryNode);
    }
  }


  _displayInfoCallback(displayInfo)
  {
    let x = displayInfo.x;
    let y = displayInfo.y;
    let subsampleWidth = displayInfo.subsampleWidth;
    this.refreshRateInputNode.value = displayInfo.selectedRefreshRate;
    this.refreshRateInputNode.max = displayInfo.maxRefreshRate;

    let limitedBy = t((displayInfo.maxRefreshRate < displayInfo.displayRefreshRate) ? "Hue bridge limit" : "display refresh rate");
    this.refreshRateLimitNode.textContent = t("(max {max} Hz: {limitedBy}. Display: {display} Hz, bridge: {bridge} Hz)", {
      max : displayInfo.maxRefreshRate,
      limitedBy : limitedBy,
      display : displayInfo.displayRefreshRate,
      bridge : displayInfo.bridgeMaxRefreshRate
    });
    this.transitionSmoothingInputNode.value = displayInfo.selectedTransitionSmoothing;
    this.transitionSmoothingLabelNode.textContent = Math.round(displayInfo.selectedTransitionSmoothing);
    this._highlightSmoothingPreset(displayInfo.selectedTransitionSmoothing);

    this.highlightBoostInputNode.value = displayInfo.selectedHighlightBoost;
    this.highlightBoostLabelNode.textContent = Math.round(displayInfo.selectedHighlightBoost);
    this._highlightPreset(this.boostPresets, displayInfo.selectedHighlightBoost);
    this.audioReactiveNode.checked = displayInfo.selectedAudioReactive;

    this.brightnessInputNode.value = displayInfo.selectedBrightness;
    this.brightnessLabelNode.textContent = Math.round(displayInfo.selectedBrightness);

    this.screenWidget.setDimensions(x, y, subsampleWidth, displayInfo.monitors);
    // The lamps may have arrived before the monitors
    this._redrawScreenWidget();
    this._monitorsCallback(displayInfo);
    let subsampleResolutionCandidates = displayInfo.subsampleResolutionCandidates;

    this.availableSubsamplesNode.innerHTML = "";
    for(let subsampleCandidate of subsampleResolutionCandidates.reverse()){
      let newOption = document.createElement("option");
      let width = subsampleCandidate.x;
      let height = subsampleCandidate.y;
      let percentage = width / this.screenWidget.width * 100;
      percentage = MathUtils.roundPrecision(percentage, 2);
      newOption.innerHTML = width + "x" + height;
      newOption.innerHTML += ` (${percentage}%)`;
      newOption.value = width;
      this.availableSubsamplesNode.appendChild(newOption);
    }
    
    this.availableSubsamplesNode.value = subsampleWidth.toString();
  }


  _refreshProcessingInfo()
  {
    if(!this.syncEnabled){
      this.processingInfoNode.textContent = t("Capture paused · static scene");
      this.statusDotNode.classList.remove("gpu", "cpu");
      return;
    }

    RequestUtils.get("/api/processingInfo")
    .then((processingInfo) => {
      if(!this.syncEnabled){
        return;
      }

      this.processingInfoNode.textContent = `${processingInfo.backend} · ${processingInfo.details}`;
      this.statusDotNode.classList.toggle("gpu", processingInfo.backend == "GPU");
      this.statusDotNode.classList.toggle("cpu", processingInfo.backend != "GPU");
    })
    .catch((error) => {log(error);});
  }


  _entertainmentConfigurationsCallback(entertainmentConfigurationsData)
  {
    let entertainmentConfigurations = entertainmentConfigurationsData.entertainmentConfigurations;
    let currentEntertainmentConfigurationId = entertainmentConfigurationsData.currentEntertainmentConfigurationId;

    if(entertainmentConfigurations.length > 1){
      this.entertainmentConfigurationsNode.style.display = "block";
    }

    for(let entertainmentConfiguration of entertainmentConfigurations){
      let newOption = document.createElement("option");
      newOption.textContent = entertainmentConfiguration.name;
      newOption.value = entertainmentConfiguration.entertainmentConfigurationId;
      this.entertainmentConfigurationsSelectNode.appendChild(newOption);
    }

    this.entertainmentConfigurationsSelectNode.value = currentEntertainmentConfigurationId;
  }


  _manageChannel(channelId)
  {
    for(let loopChannelId of Object.keys(this.activeChannels)){
      let channelNode = this.activeChannels[loopChannelId].node;
      if(loopChannelId != channelId){
        this._setItemSelected(channelNode, false);
      }
      else{
        this._toggleClicked(channelNode);
      }
    }

    if(!this.activeChannels[channelId].node.selected){
      this.screenWidget.setLegend(ScreenWidget.Legends.pleaseSelect);
      this.screenWidget.showWidgets(false);
      this.screenWidget.showPreview();
      this.screenWidget.currentChannel = null;

      return;
    }
    else{
      this.screenWidget.showPreview(channelId);
    }

    this.screenWidget.setLegend(ScreenWidget.Legends.none);

    let channelPromise = RequestUtils.get(`/api/channel/${channelId}`);
    channelPromise.then((channelData) => {
      this.activeChannels[channelId].uvs = channelData.uvs;
      this.activeChannels[channelId].monitor = channelData.monitor;
      this.screenWidget.initChannelRegion(this.activeChannels[channelId]);
    });
    channelPromise.catch((error) => {log(error);});
  }


  _setEntertainmentConfiguration(entertainmentConfigurationId)
  {
    this._showLoading(true, t("Switching configuration..."));
    let promise = RequestUtils.put("/api/setEntertainmentConfiguration", JSON.stringify(entertainmentConfigurationId));

    promise.then((entertainmentConfigurationData) => {
      this._refreshChannelsLists(entertainmentConfigurationData.channels)
      this.screenWidget.showPreview();
      this.screenWidget.showWidgets(false);
      this._showLoading(false);
    });
    promise.catch((error) => {
      log(error);
      this._showLoading(false);

      // Back to the area actually streamed to
      RequestUtils.get("/api/entertainmentConfigurations")
      .then((data) => {this._entertainmentConfigurationsCallback(data);})
      .catch((error) => {log(error);});
    });
  }


  // Starts the preview, or stops the running one
  _togglePreview()
  {
    if(this.previewEnd){
      RequestUtils.delete("/api/preview", JSON.stringify(null))
      .then((data) => {this.showPreviewRunning(data.remaining);})
      .catch((error) => {log(error);});
      return;
    }

    RequestUtils.post("/api/preview", JSON.stringify(null))
    .then((data) => {this.showPreviewRunning(data.duration);})
    .catch((error) => {log(error);});
  }


  /**
   * Counts down on the preview button while a preview runs
   */
  showPreviewRunning(remaining)
  {
    clearTimeout(this.previewTimer);
    this.previewEnd = remaining > 0 ? Date.now() + remaining * 1000 : null;

    let tick = () => {
      let left = this.previewEnd ? Math.ceil((this.previewEnd - Date.now()) / 1000) : 0;
      let running = left > 0;

      this.previewButton.classList.toggle("running", running);
      this.previewButton.textContent = running ? t("Stop preview · {left} s", {left : left}) : t("Preview");
      this.previewButton.title = running ? t("Stops the preview: the lamps go back to the screen or the static scene") : this.previewButtonTitle;

      if(!running){
        this.previewEnd = null;
        return;
      }

      this.previewTimer = setTimeout(tick, 250);
    };

    tick();
  }


  _refreshLampColors()
  {
    // One request at a time, about 4 times per second
    // Nothing to show while the page is hidden: resumed by the visibilitychange listener
    if(document.hidden){
      this.lampColorsPaused = true;
      return;
    }

    RequestUtils.get("/api/currentColors")
    .then((channelColors) => {
      for(let channelColor of channelColors){
        let rgb = ColorUtils.xyBrightnessToRgb(channelColor.r, channelColor.g, channelColor.b);
        let color = `rgb(${rgb.r}, ${rgb.g}, ${rgb.b})`;

        // Every style change repaints: skip lamps whose color didn't change
        if(this.lampColors[channelColor.channelId] == color){
          continue;
        }
        this.lampColors[channelColor.channelId] = color;

        let swatch = this.lampSwatches[channelColor.channelId];
        if(swatch){
          let glow = Math.round(4 + 10 * Math.min(1, channelColor.b));
          swatch.style.background = color;
          swatch.style.boxShadow = `0 0 ${glow}px ${Math.round(glow / 3)}px rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, 0.6)`;
        }

        this.screenWidget.setRegionColor(channelColor.channelId, `rgba(${rgb.r}, ${rgb.g}, ${rgb.b}, 0.55)`);
      }
    })
    .catch((error) => {log(error);})
    .finally(() => {setTimeout(() => {this._refreshLampColors();}, WebUI.LampColorsInterval);});
  }


  _redrawScreenWidget()
  {
    this.screenWidget.layout();

    let currentChannel = this.screenWidget.currentChannel;
    this.screenWidget.showPreview(currentChannel ? currentChannel.channelId : null);

    if(currentChannel && currentChannel.uvs){
      this.screenWidget.uvCallback(currentChannel.uvs);
    }
  }


  _setSubsampleWidth(subsampleWidth)
  {
    let promise = RequestUtils.put("/api/setSubsampleWidth", JSON.stringify(subsampleWidth));
    promise.then((displayInfo) => {
      this.screenWidget.setDimensions(displayInfo.x, displayInfo.y, displayInfo.subsampleWidth);
      this._redrawScreenWidget();
    });
    promise.catch((error) => {log(error);});
  }


  _setRefreshRate(refreshRate){
    let promise = RequestUtils.put("/api/setRefreshRate", JSON.stringify(refreshRate));
    promise.then((data) => {this._setRefreshRateCallback(data.refreshRate);});
    promise.catch((error) => {log(error);});
  }


  _setRefreshRateCallback(refreshRate)
  {
    this.refreshRateInputNode.value = refreshRate;
  }


  _highlightSmoothingPreset(transitionSmoothing)
  {
    this._highlightPreset(this.smoothingPresets, transitionSmoothing);
  }


  _highlightPreset(presets, value)
  {
    for(let preset of presets){
      preset.button.classList.toggle("active", Math.round(value) == preset.value);
    }
  }


  _setBrightness(brightness)
  {
    RequestUtils.put("/api/setBrightness", JSON.stringify(brightness))
    .then((data) => {
      this.brightnessInputNode.value = data.brightness;
      this.brightnessLabelNode.textContent = Math.round(data.brightness);
    })
    .catch((error) => {log(error);});
  }


  _setHighlightBoost(highlightBoost)
  {
    RequestUtils.put("/api/setHighlightBoost", JSON.stringify(highlightBoost))
    .then((data) => {
      this.highlightBoostInputNode.value = data.highlightBoost;
      this.highlightBoostLabelNode.textContent = Math.round(data.highlightBoost);
      this._highlightPreset(this.boostPresets, data.highlightBoost);
    })
    .catch((error) => {log(error);});
  }


  _setAudioReactive(audioReactive)
  {
    RequestUtils.put("/api/setAudioReactive", JSON.stringify(audioReactive))
    .then((data) => {this.audioReactiveNode.checked = data.audioReactive;})
    .catch((error) => {log(error);});
  }


  _setTransitionSmoothing(transitionSmoothing){
    let promise = RequestUtils.put("/api/setTransitionSmoothing", JSON.stringify(transitionSmoothing));
    promise.then((data) => {this._setTransitionSmoothingCallback(data.transitionSmoothing);});
    promise.catch((error) => {log(error);});
  }


  _setTransitionSmoothingCallback(transitionSmoothing)
  {
    this.transitionSmoothingInputNode.value = transitionSmoothing;
    this.transitionSmoothingLabelNode.textContent = Math.round(transitionSmoothing);
    this._highlightSmoothingPreset(transitionSmoothing);
  }


  _saveProfile()
  {
    RequestUtils.post("/api/saveProfile", JSON.stringify(null), (data) => {log("Saved profile");});
  }


  // Until the current version of the notice is accepted, it covers the light manager
  _checkNotice()
  {
    RequestUtils.get("/api/notice")
    .then((data) => {
      if(!data.accepted){
        this._showNotice();
      }
    })
    .catch((error) => {log(error);});
  }


  _showNotice()
  {
    let section = document.getElementById("noticeSection");
    let acceptButton = document.getElementById("acceptNoticeButton");

    Notice.render(document.getElementById("notice"), (confirmed) => {acceptButton.disabled = !confirmed;});
    acceptButton.onclick = () => {
      RequestUtils.put("/api/acceptNotice", JSON.stringify(null))
      .then((data) => {
        if(data.accepted){
          section.style.display = "none";
          this.overlay.classList.remove("notice");
          this.overlay.style.display = "none";
        }
      })
      .catch((error) => {log(error);});
    };

    this.overlay.classList.add("notice");
    this.overlay.style.display = "block";
    section.style.display = "block";
  }


  _askStopConfirmation()
  {
    this.overlay.style.display = "block";
    document.getElementById("confirmStopSection").style.display = "block";
  }


  _stop()
  {
    let promise = RequestUtils.post("/api/stop", JSON.stringify(null));
    promise.then((data) => {
      if(data.succeeded){
        document.getElementById("confirmStopSection").style.display = "none";
        document.getElementById("stoppedInfoSection").style.display = "block";
      }
    });
  }


  _showLoading(show, loadingText = null)
  {
    let loadingInfoSectionNode = document.getElementById("loadingInfoSection");

    if(show){
      this.overlay.style.display = "block";
      loadingInfoSectionNode.style.display = "block";
      if(loadingText !== null){
        document.getElementById("loadingTextInfo").textContent = loadingText;
      }
    }
    else{
      this.overlay.style.display = "none";
      loadingInfoSectionNode.style.display = "none";
    }
  }
}


// Interval between live lamp color updates [ms]. Each update costs a frame in the web engine:
// 4 per second keep the dots live at about half the CPU time of 8
WebUI.LampColorsInterval = 250;

// Transition smoothing presets [%]
WebUI.GamingSmoothing = 10;
WebUI.MovieSmoothing = 60;
