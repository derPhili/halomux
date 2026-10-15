// Without sync, each lamp shows a static color. Built-in scenes spread a left and a right color over
// the lamps, ordered by the horizontal position of their screen region.
class StaticScenePanel
{
  constructor(webUI)
  {
    this.webUI = webUI;

    this.syncEnabled = true;
    // "#rrggbb" by channel ID
    this.staticColors = {};
    // Saved scenes: static colors by name
    this.scenes = {};
    // Built-in scenes from the backend: name, left and right color
    this.builtinScenes = [];

    // Only one color update is in flight and only the newest waits, so dragging a color picker
    // doesn't flood the backend
    this.pendingColors = null;
    this.colorsRequestInFlight = false;

    this.syncToggleNode = document.getElementById("syncToggle");
    this.syncLabelNode = document.getElementById("syncLabel");
    this.syncToggleNode.addEventListener("change", (event) => {this._setSyncEnabled(event.target.checked);});

    this.sceneHintNode = document.getElementById("sceneHint");

    this.fullColorTab = document.getElementById("fullColorTab");
    this.perLampTab = document.getElementById("perLampTab");
    this.fullColorPanel = document.getElementById("fullColorPanel");
    this.perLampPanel = document.getElementById("perLampPanel");
    this.fullColorTab.addEventListener("click", () => {this._showTab(false);});
    this.perLampTab.addEventListener("click", () => {this._showTab(true);});

    this.fullColorNode = document.getElementById("fullColor");
    this.fullColorNode.addEventListener("input", (event) => {this._applyFullColor(event.target.value);});

    this.quickColorsNode = document.getElementById("quickColors");
    for(let color of StaticScenePanel.QuickColors){
      let button = document.createElement("button");
      button.type = "button";
      button.className = "quickColor";
      button.style.background = color.value;
      button.title = t("{name}: shows this color on all lamps and turns sync off", {name : t(color.name)});
      button.dataset.color = color.value;
      button.addEventListener("click", () => {this._applyFullColor(color.value);});
      this.quickColorsNode.appendChild(button);
    }

    this.builtinScenesNode = document.getElementById("builtinScenes");
    this.customScenesNode = document.getElementById("customScenes");

    this.sceneNameNode = document.getElementById("sceneName");
    document.getElementById("saveSceneForm").addEventListener("submit", (event) => {
      event.preventDefault();
      this._saveScene(this.sceneNameNode.value);
    });

    this._showTab(false);
  }


  init()
  {
    RequestUtils.get("/api/lightMode")
    .then((lightMode) => {
      this.scenes = lightMode.scenes;
      this.builtinScenes = lightMode.builtinScenes;
      // A preview started elsewhere (tray) or before a reload
      this.webUI.showPreviewRunning(lightMode.previewRemaining);
      this._renderBuiltinScenes();
      this._renderCustomScenes();
      this._updateSync(lightMode.syncEnabled);
      this._updateStaticColors(lightMode.staticColors);
    })
    .catch((error) => {log(error);});
  }


  /**
   * To call when the active lamps or their regions changed
   */
  refreshLamps()
  {
    this._renderPerLampPanel();
    this._renderCustomScenes();
    this._highlightCurrent();
  }


  // Active lamps from left to right
  _lamps()
  {
    let center = (channel) => channel.uvs ? (channel.uvs.uvA.x + channel.uvs.uvB.x) / 2 : 0.5;
    return Object.values(this.webUI.activeChannels).sort((a, b) => center(a) - center(b));
  }


  _colorOf(channelId)
  {
    return this.staticColors[channelId] || StaticScenePanel.DefaultColor;
  }


  _showTab(perLamp)
  {
    this.fullColorTab.classList.toggle("active", !perLamp);
    this.perLampTab.classList.toggle("active", perLamp);
    this.fullColorPanel.style.display = perLamp ? "none" : "";
    this.perLampPanel.style.display = perLamp ? "" : "none";
  }


  _renderPerLampPanel()
  {
    this.perLampPanel.replaceChildren();

    let lamps = this._lamps();
    if(lamps.length == 0){
      let hint = document.createElement("span");
      hint.className = "hint";
      hint.textContent = t("No active lamp");
      this.perLampPanel.appendChild(hint);
      return;
    }

    for(let lamp of lamps){
      let field = document.createElement("label");
      field.className = "colorField";
      field.title = t("Color of {lamp} while sync is off. Picking one turns sync off", {lamp : lamp.formatMembers()});

      let input = document.createElement("input");
      input.type = "color";
      input.value = this._colorOf(lamp.channelId);
      input.dataset.channelId = lamp.channelId;
      input.addEventListener("input", (event) => {
        this._applyColors({...this._currentLampColors(), [lamp.channelId] : event.target.value});
      });

      let name = document.createElement("span");
      name.textContent = lamp.formatMembers();

      field.replaceChildren(input, name);
      this.perLampPanel.appendChild(field);
    }
  }


  // Static colors of the active lamps only
  _currentLampColors()
  {
    let colors = {};
    for(let lamp of this._lamps()){
      colors[lamp.channelId] = this._colorOf(lamp.channelId);
    }

    return colors;
  }


  // Spreads the colors of a built-in scene over the lamps, from left to right, as the backend does
  _spread(left, right)
  {
    let lamps = this._lamps();
    let colors = {};

    lamps.forEach((lamp, index) => {
      let t = lamps.length > 1 ? index / (lamps.length - 1) : 0;
      colors[lamp.channelId] = StaticScenePanel.mixHex(left, right, t);
    });

    return colors;
  }


  _sceneButton(name, background, onClick, tooltip)
  {
    let item = document.createElement("div");
    item.className = "sceneItem";

    let button = document.createElement("button");
    button.type = "button";
    button.className = "sceneButton";
    button.title = tooltip;
    button.addEventListener("click", onClick);

    let swatch = document.createElement("span");
    swatch.className = "sceneSwatch";
    swatch.style.background = background;

    let label = document.createElement("span");
    label.className = "sceneName";
    label.textContent = name;

    button.replaceChildren(swatch, label);
    item.appendChild(button);

    return item;
  }


  _renderBuiltinScenes()
  {
    this.builtinScenesNode.replaceChildren();

    for(let scene of this.builtinScenes){
      let name = t(scene.name);
      let tooltip = scene.left == scene.right
        ? t("{name}: {color} on all lamps. Turns sync off", {name : name, color : scene.left})
        : t("{name}: from {left} on the leftmost lamp to {right} on the rightmost. Turns sync off", {name : name, left : scene.left, right : scene.right});
      let item = this._sceneButton(name, StaticScenePanel.swatchBackground([scene.left, scene.right]), () => {
        this._applyScene(scene.name, true);
      }, tooltip);
      item.dataset.left = scene.left;
      item.dataset.right = scene.right;
      this.builtinScenesNode.appendChild(item);
    }
  }


  _renderCustomScenes()
  {
    this.customScenesNode.replaceChildren();

    let lampIds = this._lamps().map(lamp => lamp.channelId.toString());

    for(let [name, colors] of Object.entries(this.scenes)){
      // Lamps ordered from left to right, then lamps that aren't active anymore
      let ordered = lampIds.filter(id => id in colors).map(id => colors[id]);
      if(ordered.length == 0){
        ordered = Object.values(colors);
      }

      let item = this._sceneButton(name, StaticScenePanel.swatchBackground(ordered), () => {
        this._applyScene(name, false);
      }, t("{name}: shows your saved colors on the lamps it covers. Turns sync off", {name : name}));
      item.dataset.scene = name;

      let deleteButton = document.createElement("button");
      deleteButton.type = "button";
      deleteButton.className = "deleteScene";
      deleteButton.title = t("Deletes the saved scene \"{name}\", after asking", {name : name});
      deleteButton.textContent = "×";
      deleteButton.addEventListener("click", () => {
        if(confirm(t("Delete the scene \"{name}\"?", {name : name}))){
          this._deleteScene(name);
        }
      });

      item.appendChild(deleteButton);
      this.customScenesNode.appendChild(item);
    }
  }


  // Marks the controls matching the current static colors
  _highlightCurrent()
  {
    let current = this._currentLampColors();
    let values = Object.values(current);
    let uniform = values.length > 0 && values.every(color => color == values[0]);

    if(uniform){
      this.fullColorNode.value = values[0];
    }

    for(let button of this.quickColorsNode.children){
      button.classList.toggle("active", uniform && button.dataset.color == values[0]);
    }

    for(let input of this.perLampPanel.querySelectorAll("input[type=color]")){
      if(document.activeElement != input){
        input.value = this._colorOf(input.dataset.channelId);
      }
    }

    let matches = (colors) => values.length > 0 && Object.keys(current).every(id => colors[id] == current[id]);

    for(let item of this.builtinScenesNode.children){
      item.firstChild.classList.toggle("active", matches(this._spread(item.dataset.left, item.dataset.right)));
    }

    for(let item of this.customScenesNode.children){
      item.firstChild.classList.toggle("active", matches(this.scenes[item.dataset.scene]));
    }
  }


  _applyFullColor(color)
  {
    let colors = {};
    for(let lamp of this._lamps()){
      colors[lamp.channelId] = color;
    }

    this._applyColors(colors);
  }


  // Shows colors on the lamps: picking a color or a scene turns sync off
  _applyColors(colors)
  {
    this._updateStaticColors({...this.staticColors, ...colors});

    this.pendingColors = this.staticColors;
    if(!this.colorsRequestInFlight){
      this._sendPendingColors();
    }
  }


  // The backend spreads built-in scenes over the lamps and turns sync off
  _applyScene(name, builtin)
  {
    RequestUtils.put("/api/applyScene", JSON.stringify({name : name, builtin : builtin}))
    .then((data) => {
      this._updateStaticColors(data.staticColors);
      this._updateSync(data.syncEnabled);
    })
    .catch((error) => {log(error);});
  }


  _sendPendingColors()
  {
    if(this.pendingColors === null){
      return;
    }

    let colors = this.pendingColors;
    this.pendingColors = null;
    this.colorsRequestInFlight = true;

    RequestUtils.put("/api/setStaticColors", JSON.stringify(colors))
    .then(() => {
      if(this.syncEnabled){
        this._setSyncEnabled(false);
      }
    })
    .catch((error) => {log(error);})
    .finally(() => {
      this.colorsRequestInFlight = false;
      this._sendPendingColors();
    });
  }


  _updateStaticColors(staticColors)
  {
    this.staticColors = staticColors;
    this._highlightCurrent();
  }


  _setSyncEnabled(syncEnabled)
  {
    this._updateSync(syncEnabled);

    RequestUtils.put("/api/setSyncEnabled", JSON.stringify(syncEnabled))
    .then((data) => {this._updateSync(data.syncEnabled);})
    .catch((error) => {log(error);});
  }


  _updateSync(syncEnabled)
  {
    this.syncEnabled = syncEnabled;
    this.syncToggleNode.checked = syncEnabled;
    this.syncLabelNode.textContent = t(syncEnabled ? "Sync" : "Sync off");
    this.sceneHintNode.textContent = t(syncEnabled ? "Pick one to turn sync off" : "Showing now");
    this.webUI.onSyncChanged(syncEnabled);
  }


  _saveScene(name)
  {
    name = name.trim();
    if(name.length == 0){
      return;
    }

    RequestUtils.put("/api/saveScene", JSON.stringify({name : name, colors : this._currentLampColors()}))
    .then((data) => {
      this.scenes = data.scenes;
      this.sceneNameNode.value = "";
      this._renderCustomScenes();
      this._highlightCurrent();
    })
    .catch((error) => {log(error);});
  }


  _deleteScene(name)
  {
    RequestUtils.delete("/api/scene", JSON.stringify({name : name}))
    .then((data) => {
      this.scenes = data.scenes;
      this._renderCustomScenes();
      this._highlightCurrent();
    })
    .catch((error) => {log(error);});
  }


  // Diagonal bands, one per color
  static swatchBackground(colors)
  {
    if(colors.length == 0){
      return StaticScenePanel.DefaultColor;
    }

    let step = 100 / colors.length;
    let bands = colors.map((color, index) => `${color} ${index * step}% ${(index + 1) * step}%`);
    return `linear-gradient(135deg, ${bands.join(", ")})`;
  }


  static mixHex(a, b, t)
  {
    let channel = (hex, index) => parseInt(hex.substr(1 + index * 2, 2), 16);
    let mixed = [0, 1, 2].map(index => Math.round(channel(a, index) * (1 - t) + channel(b, index) * t));
    return "#" + mixed.map(value => value.toString(16).padStart(2, "0")).join("");
  }
}


// Shown by lamps without a static color yet, as in the backend
StaticScenePanel.DefaultColor = "#ffb46e";

StaticScenePanel.QuickColors = [
  {name : "Red", value : "#ff1e1e"},
  {name : "Orange", value : "#ff7a00"},
  {name : "Yellow", value : "#ffd200"},
  {name : "Green", value : "#28dc50"},
  {name : "Cyan", value : "#00d2d2"},
  {name : "Blue", value : "#1e50ff"},
  {name : "Purple", value : "#8c32ff"},
  {name : "Pink", value : "#ff32b4"},
  {name : "Warm white", value : "#ffb46e"},
  {name : "Cool white", value : "#e6eeff"}
];
