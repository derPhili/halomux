class Utils
{
  static clamp(value, min, max)
  {
    return Math.max(min, Math.min(value, max));
  }


  static truncate(value, decimals)
  {
    let exp = Math.pow(10, decimals);
    return Math.round((value) * exp) / exp;
  }
}


class Handle
{
  static Type =
  {
    TopLeft : 0,
    TopRight : 1,
    BottomLeft : 2,
    BottomRight : 3
  };


  static MapHandleTypeId =
  {
    "tl" : Handle.Type.TopLeft,
    "tr" : Handle.Type.TopRight,
    "bl" : Handle.Type.BottomLeft,
    "br" : Handle.Type.BottomRight
  };


  constructor(handleId, screenWidget, parentNode)
  {
    this.handleNode = document.getElementById(handleId);
    this.screenWidget = screenWidget;
    this.parentNode = parentNode;
    this.handleNode.addEventListener("mousedown", () => {this.drag();});
    this.type = Handle.MapHandleTypeId[handleId];
  }


  drag()
  {
    this.screenWidget.draggedHandle = this;
  }


  drop(){}


  setPosition(position, notify = false)
  {
    // Corners stay on the monitor of the region, on its subsample grid
    let monitor = this.screenWidget.currentMonitorRect();
    if(!monitor){
      return;
    }

    let columns = monitor.subsampleWidth;
    let rows = monitor.subsampleHeight;

    let xNorm = Math.round(Utils.clamp((position.x - monitor.x) / monitor.width, 0, 1) * columns) / columns;
    let yNorm = Math.round(Utils.clamp((position.y - monitor.y) / monitor.height, 0, 1) * rows) / rows;

    this.handleNode.setAttribute("cx", monitor.x + xNorm * monitor.width);
    this.handleNode.setAttribute("cy", monitor.y + yNorm * monitor.height);

    let uvData = {
      x : xNorm,
      y : yNorm,
      type : this.type
    };

    if(notify){
      this.screenWidget._updateUVs(uvData);
    }
  }
}


class GammaHandle
{
  constructor(screenWidget, parentNode)
  {
    this.screenWidget = screenWidget;
    this.parentNode = parentNode;
    this.handleNode = document.getElementById("gammaCursor");
    this.handleNode.addEventListener("mousedown", () => {this.drag();});
  }


  drag()
  {
    this.screenWidget.draggedHandle = this;
  }


  affectFactor(factor)
  {
    factor += 1;
    factor /= 2
    factor *= this.screenWidget.gammaAreaDimensions().x;

    this.setPosition({x : factor});
  }


  setPosition(position)
  {
    let areaDimensions = this.screenWidget.gammaAreaDimensions();
    let gammaFactor =  Utils.clamp(position.x / areaDimensions.x, 0, 1);

    gammaFactor = Utils.truncate(gammaFactor, 1)

    this.handleNode.setAttribute("cx", `${gammaFactor * 100}%`);

    gammaFactor *= 2;
    gammaFactor -= 1;
    gammaFactor = Utils.truncate(gammaFactor, 1)

    this.screenWidget.webApp.updateGammaFactor(gammaFactor);
  }


  drop(){}
}


class Rectangle
{
  constructor(rectangleNode)
  {
    this.svgNode = rectangleNode;
  }


  setGeometry(x, y, width, height)
  {
    this.svgNode.setAttribute("x", x);
    this.svgNode.setAttribute("y", y);
    this.svgNode.setAttribute("width", width);
    this.svgNode.setAttribute("height", height);
  }


  show(show = true)
  {
    let display = show ? "block" : "none";
    this.svgNode.style.display = display;
  }
}


class ScreenWidget
{
  static Legends = {
    none : "",
    noChannel : "There are no lamps in this entertainment area. Add them in the Hue app.",
    pleaseDrag : "Drag a lamp from Available to Active to drive it.",
    pleaseSelect : "Select an active lamp to edit its region"
  };


  constructor(webApp)
  {
    this.webApp = webApp;
    this.svgAreaNode = document.getElementById("svgArea");
    this.screenAreaNode = document.getElementById("screenArea");
    this.monitorsAreaNode = document.getElementById("monitorsArea");
    this.gammaAreaNode = document.getElementById("gammaArea");
    this.uvAreaNode = document.getElementById("uvArea");
    this.uvRectangle = new Rectangle(this.uvAreaNode);
    this.svgChannelMembersNamesNode = document.getElementById("svgChannelName");
    this.svgChannelUVSizeNode = document.getElementById("svgChannelUVSize");
    this.previewArea = document.getElementById("previewRectangles");
    this.legendText = document.getElementById("legendText");

    this.gammaCursor = new GammaHandle(this, this.gammaAreaNode);

    let handleIds = ["tl", "tr", "bl", "br"];
    this.handles = {}

    for(let handleId of handleIds){
      this.handles[handleId] = new Handle(handleId, this, this.screenAreaNode);
    };

    // Dragging the region itself moves it, onto another monitor too
    this.uvAreaNode.querySelector("rect").addEventListener("mousedown", (event) => {this._startMove(event);});

    document.addEventListener("mouseup", () => {this.drop();})
    this.svgAreaNode.addEventListener("mousemove", (event) => {this._updateMousePosition(event);});
    this.svgAreaNode.addEventListener("mouseup", () => {this.drop();});

    this.draggedHandle = null;
    this.move = null;
    this.currentChannel = null;

    // Captured monitors as /api/displayInfo describes them, and where each one is drawn
    this.monitors = [];
    this.monitorRects = [];

    // Live lamp colors shown in the regions of the lamps not being edited
    this.previewRegions = {};
    this.regionColors = {};
  }


  screenWidgetResolution()
  {
    let bbox = this.screenAreaNode.getBoundingClientRect();
    return {x : bbox.width, y : bbox.height};
  }


  gammaAreaDimensions()
  {
    let bbox = this.gammaAreaNode.getBoundingClientRect();
    return {x : bbox.width, y : bbox.height};
  }


  initChannelRegion(channel)
  {
    this.currentChannel = channel;
    this.showWidgets(true);
    this._updateShape(channel.uvs);

    this.svgChannelMembersNamesNode.innerHTML = channel.formatMembers();
  }


  setLegend(legendFlag){
    this.legendText.textContent = legendFlag ? t(legendFlag) : "";
  }


  /**
   * Sets the captured monitors
   *
   * @param width Pixel width of the widest monitor, the one subsampleWidth is for
   * @param height Its pixel height
   * @param subsampleWidth Configured subsample width
   * @param monitors Captured monitors, as /api/displayInfo describes them, or null to keep them
   */
  setDimensions(width, height, subsampleWidth, monitors = null)
  {
    this.width = width;
    this.height = height;
    this.subsampleWidth = subsampleWidth;

    if(monitors){
      this.monitors = monitors;
    }

    if(this.monitors.length == 0){
      // Before the monitors are known: one monitor of the given size
      this.monitors = [{index : 0, x : 0, y : 0, width : width, height : height, pixelWidth : width, pixelHeight : height, description : "", active : true}];
    }

    this.layout();
  }


  /**
   * Places the monitors as they are arranged on the desktop, and draws them
   */
  layout()
  {
    let area = this.screenWidgetResolution();
    this.monitorRects = [];

    if(this.monitors.length == 0 || area.x == 0 || area.y == 0){
      return;
    }

    let left = Math.min(...this.monitors.map(monitor => monitor.x));
    let top = Math.min(...this.monitors.map(monitor => monitor.y));
    let right = Math.max(...this.monitors.map(monitor => monitor.x + monitor.width));
    let bottom = Math.max(...this.monitors.map(monitor => monitor.y + monitor.height));

    // A single monitor fills the area, as before monitors were chosen per lamp. Several keep
    // their proportions and arrangement.
    let scaleX = area.x / (right - left);
    let scaleY = area.y / (bottom - top);
    let offsetX = 0;
    let offsetY = 0;

    if(this.monitors.length > 1){
      let scale = Math.min(scaleX, scaleY);
      scaleX = scale;
      scaleY = scale;
      offsetX = (area.x - (right - left) * scale) / 2;
      offsetY = (area.y - (bottom - top) * scale) / 2;
    }

    let widestPixelWidth = Math.max(...this.monitors.map(monitor => monitor.pixelWidth));

    for(let monitor of this.monitors){
      // Same subsample ratio on every monitor, as the service computes it
      let subsampleWidth = Utils.clamp(Math.round(this.subsampleWidth * monitor.pixelWidth / widestPixelWidth), 1, monitor.pixelWidth);

      this.monitorRects.push({
        index : monitor.index,
        x : offsetX + (monitor.x - left) * scaleX,
        y : offsetY + (monitor.y - top) * scaleY,
        width : monitor.width * scaleX,
        height : monitor.height * scaleY,
        subsampleWidth : subsampleWidth,
        subsampleHeight : Math.max(1, Math.round(monitor.pixelHeight * subsampleWidth / monitor.pixelWidth))
      });
    }

    this._drawMonitors();
  }


  /**
   * Returns where the monitor of the region being edited is drawn
   */
  currentMonitorRect()
  {
    if(!this.currentChannel || this.currentChannel.monitor === null){
      return null;
    }

    return this.monitorRects[this.currentChannel.monitor] || null;
  }


  _drawMonitors()
  {
    const svg = "http://www.w3.org/2000/svg";
    this.monitorsAreaNode.innerHTML = "";

    for(let i = 0; i < this.monitorRects.length; i++){
      let rect = this.monitorRects[i];
      let monitor = this.monitors[i];

      let group = document.createElementNS(svg, "svg");
      group.setAttribute("x", rect.x);
      group.setAttribute("y", rect.y);
      group.setAttribute("width", rect.width);
      group.setAttribute("height", rect.height);
      group.classList.add("monitor");
      group.classList.toggle("idle", this.monitors.length > 1 && !monitor.active);

      let screen = document.createElementNS(svg, "rect");
      screen.setAttribute("width", "100%");
      screen.setAttribute("height", "100%");
      screen.classList.add("monitorScreen");
      group.appendChild(screen);

      // Grid of the subsample the regions are averaged on, when it isn't too dense to see
      if(rect.width / rect.subsampleWidth >= 2){
        for(let column = 0; column < rect.subsampleWidth; column++){
          let line = document.createElementNS(svg, "line");
          let x = `${column / rect.subsampleWidth * 100}%`;
          line.setAttribute("x1", x);
          line.setAttribute("x2", x);
          line.setAttribute("y1", "0%");
          line.setAttribute("y2", "100%");
          line.classList.add("gridLine");
          group.appendChild(line);
        }

        for(let row = 0; row < rect.subsampleHeight; row++){
          let line = document.createElementNS(svg, "line");
          let y = `${row / rect.subsampleHeight * 100}%`;
          line.setAttribute("x1", "0%");
          line.setAttribute("x2", "100%");
          line.setAttribute("y1", y);
          line.setAttribute("y2", y);
          line.classList.add("gridLine");
          group.appendChild(line);
        }
      }

      if(this.monitors.length > 1){
        let label = document.createElementNS(svg, "text");
        label.textContent = monitor.description;
        label.setAttribute("x", "98%");
        label.setAttribute("y", "95%");
        label.setAttribute("text-anchor", "end");
        label.classList.add("monitorLabel");
        group.appendChild(label);

        let title = document.createElementNS(svg, "title");
        title.textContent = monitor.active
          ? t("{monitor}: captured, a lamp follows it", {monitor : monitor.description})
          : t("{monitor}: no lamp follows it, so it isn't captured", {monitor : monitor.description});
        group.appendChild(title);
      }

      this.monitorsAreaNode.appendChild(group);
    }
  }


  showWidgets(show)
  {
    let display = show ? "block" : "none";
    this.uvRectangle.show(show);
    for(let [key, handle] of Object.entries(this.handles)){
      handle.handleNode.style.display = display;
    }

    this.gammaAreaNode.style.display = display;
    if(show){
      this.gammaCursor.affectFactor(this.currentChannel.gammaFactor);
    }
  }


  showPreview(exceptedChannelId = null)
  {
    this.previewArea.innerHTML = "";
    this.previewRegions = {};

    let activeChannels = this.webApp.activeChannels;

    if(!activeChannels){
      return;
    }

    for(let activeChannelId of Object.keys(activeChannels)){
      if(activeChannelId == exceptedChannelId){
        continue;
      }

      let activeChannel = activeChannels[activeChannelId];
      let geometry = this._regionGeometry(activeChannel.monitor, activeChannel.uvs);
      if(!geometry){
        // Its monitor isn't captured
        continue;
      }

      var rectNode = document.createElementNS("http://www.w3.org/2000/svg", "rect");
      var textNode = document.createElementNS("http://www.w3.org/2000/svg", "text");

      textNode.textContent = activeChannel.formatMembers();
      textNode.setAttribute("text-anchor", "middle");
      textNode.setAttribute("x", geometry.x + geometry.width / 2);
      textNode.setAttribute("y", geometry.y + geometry.height / 2);

      rectNode.classList.add("previewRegion");

      let titleNode = document.createElementNS("http://www.w3.org/2000/svg", "title");
      titleNode.textContent = t("{lamp}: the screen region this lamp follows, filled with its current color. Select the lamp in the list to edit it", {lamp : activeChannel.formatMembers()});
      rectNode.appendChild(titleNode);
      rectNode.style.fill = this.regionColors[activeChannelId] || ScreenWidget.IdleRegionFill;
      this.previewRegions[activeChannelId] = rectNode;

      new Rectangle(rectNode).setGeometry(geometry.x, geometry.y, geometry.width, geometry.height);

      this.previewArea.appendChild(rectNode);
      this.previewArea.appendChild(textNode);
    }
  }


  drop()
  {
    if(this.draggedHandle){
      this.draggedHandle.drop();
      this.draggedHandle = null;
    }

    if(this.move){
      this._finishMove();
    }
  }


  uvCallback(uvs)
  {
    this._updateShape(uvs);
  }


  /**
   * Returns where a region is drawn, nothing if its monitor isn't captured
   */
  _regionGeometry(monitorIndex, uvs)
  {
    if(monitorIndex === null || monitorIndex === undefined || !uvs){
      return null;
    }

    let monitor = this.monitorRects[monitorIndex];
    if(!monitor){
      return null;
    }

    let ax = monitor.x + uvs.uvA.x * monitor.width;
    let ay = monitor.y + uvs.uvA.y * monitor.height;
    let bx = monitor.x + uvs.uvB.x * monitor.width;
    let by = monitor.y + uvs.uvB.y * monitor.height;

    return {x : ax, y : ay, width : bx - ax, height : by - ay};
  }


  _updateShape(uvs)
  {
    let geometry = this._regionGeometry(this.currentChannel ? this.currentChannel.monitor : null, uvs);
    if(!geometry){
      this.showWidgets(false);
      return;
    }

    let ax = geometry.x;
    let ay = geometry.y;
    let bx = geometry.x + geometry.width;
    let by = geometry.y + geometry.height;

    this.handles["tl"].setPosition({x : ax, y : ay});
    this.handles["tr"].setPosition({x : bx, y : ay});
    this.handles["bl"].setPosition({x : ax, y : by});
    this.handles["br"].setPosition({x : bx, y : by});

    this.uvRectangle.setGeometry(ax, ay, geometry.width, geometry.height);

    let propWidth = Utils.truncate((uvs.uvB.x - uvs.uvA.x) * 100, 2);
    let propHeight = Utils.truncate((uvs.uvB.y - uvs.uvA.y) * 100, 2);

    this.svgChannelUVSizeNode.innerHTML = `${propWidth}% x ${propHeight}%`;
  }


  _localPoint(event)
  {
    let ctm = this.screenAreaNode.getScreenCTM();
    return {
      x : (event.clientX - ctm.e) / ctm.a,
      y : (event.clientY - ctm.f) / ctm.d
    };
  }


  _updateMousePosition(event)
  {
    if(this.draggedHandle){
      var ctm = this.draggedHandle.parentNode.getScreenCTM();

      let point = {
        x: (event.clientX - ctm.e) / ctm.a,
        y: (event.clientY - ctm.f) / ctm.d
      }

      this.draggedHandle.setPosition(point, true);
    }

    if(this.move){
      let point = this._localPoint(event);
      let geometry = this.move.geometry;
      this.move.x = point.x - this.move.grabX;
      this.move.y = point.y - this.move.grabY;
      this.move.moved = true;

      // The region follows the pointer freely; it lands on a monitor when dropped
      this.uvRectangle.setGeometry(this.move.x, this.move.y, geometry.width, geometry.height);
      for(let handle of Object.values(this.handles)){
        handle.handleNode.style.display = "none";
      }
    }
  }


  _startMove(event)
  {
    if(!this.currentChannel || this.draggedHandle){
      return;
    }

    let geometry = this._regionGeometry(this.currentChannel.monitor, this.currentChannel.uvs);
    if(!geometry){
      return;
    }

    let point = this._localPoint(event);
    this.move = {
      geometry : geometry,
      grabX : point.x - geometry.x,
      grabY : point.y - geometry.y,
      x : geometry.x,
      y : geometry.y,
      moved : false
    };

    event.preventDefault();
  }


  /**
   * Puts the moved region onto the monitor under its center, or the nearest one
   */
  _finishMove()
  {
    let move = this.move;
    this.move = null;

    if(!move.moved || !this.currentChannel || this.monitorRects.length == 0){
      return;
    }

    let centerX = move.x + move.geometry.width / 2;
    let centerY = move.y + move.geometry.height / 2;

    let target = null;
    let targetDistance = Infinity;
    for(let monitor of this.monitorRects){
      let dx = Math.max(monitor.x - centerX, 0, centerX - (monitor.x + monitor.width));
      let dy = Math.max(monitor.y - centerY, 0, centerY - (monitor.y + monitor.height));
      let distance = Math.hypot(dx, dy);
      if(distance < targetDistance){
        target = monitor;
        targetDistance = distance;
      }
    }

    // Same size on the desktop, cut to the monitor if larger, pushed inside it
    let width = Math.min(move.geometry.width, target.width);
    let height = Math.min(move.geometry.height, target.height);
    let x = Utils.clamp(move.x, target.x, target.x + target.width - width);
    let y = Utils.clamp(move.y, target.y, target.y + target.height - height);

    let snap = (value, steps) => Utils.clamp(Math.round(value * steps) / steps, 0, 1);
    let uvs = {
      uvA : {x : snap((x - target.x) / target.width, target.subsampleWidth), y : snap((y - target.y) / target.height, target.subsampleHeight)},
      uvB : {x : snap((x + width - target.x) / target.width, target.subsampleWidth), y : snap((y + height - target.y) / target.height, target.subsampleHeight)}
    };

    this.webApp.moveChannelRegion(this.currentChannel.channelId, target.index, uvs);
  }


  setRegionColor(channelId, color)
  {
    this.regionColors[channelId] = color;

    let region = this.previewRegions[channelId];
    if(region){
      region.style.fill = color;
    }
  }


  _updateUVs(uvData)
  {
    this.webApp.notifyUV(uvData);
  }
}


ScreenWidget.IdleRegionFill = "rgba(255, 255, 255, 0.04)";
