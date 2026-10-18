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
    this.handleNode.addEventListener("pointerdown", (event) => {event.preventDefault(); this.drag();});
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
    this.handleNode.addEventListener("pointerdown", (event) => {event.preventDefault(); this.drag();});
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
    this.handleNode.setAttribute("aria-valuenow", gammaFactor);

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
    pleaseDrag : "Drag a lamp from Available to Active, or use its arrow, to drive it.",
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
    // Pointer events: mouse, touch and pen alike
    this.uvAreaNode.querySelector("rect").addEventListener("pointerdown", (event) => {this._startMove(event);});

    document.addEventListener("pointerup", () => {this.drop();});
    document.addEventListener("pointercancel", () => {this.drop();});
    this.svgAreaNode.addEventListener("pointermove", (event) => {this._updateMousePosition(event);});
    window.addEventListener("resize", () => {this._layoutLegend();});

    // Keyboard: the region and the gamma cursor can be focused and moved with the arrow keys
    this.regionNode = this.uvAreaNode.querySelector("rect");
    this.regionNode.setAttribute("tabindex", "0");
    this.regionNode.addEventListener("keydown", (event) => {this._onRegionKey(event);});
    this.gammaCursor.handleNode.setAttribute("tabindex", "0");
    this.gammaCursor.handleNode.setAttribute("role", "slider");
    this.gammaCursor.handleNode.setAttribute("aria-valuemin", "-1");
    this.gammaCursor.handleNode.setAttribute("aria-valuemax", "1");
    this.gammaCursor.handleNode.addEventListener("keydown", (event) => {this._onGammaKey(event);});

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
    // Labels included
    this._updateShape(channel.uvs);
  }


  setLegend(legendFlag){
    this.legendFlag = legendFlag;
    this._layoutLegend();
  }


  // Below the monitors, wrapped into as many lines as the widget's width needs
  _layoutLegend()
  {
    const text = this.legendFlag ? t(this.legendFlag) : "";
    this.legendText.textContent = text;

    const available = this.svgAreaNode.getBoundingClientRect().width * 0.94;
    if(!text || available <= 0 || this.legendText.getBBox().width <= available){
      return;
    }

    this.legendText.textContent = "";
    const addLine = () => {
      const span = document.createElementNS("http://www.w3.org/2000/svg", "tspan");
      span.setAttribute("x", "50%");
      span.setAttribute("dy", "1.2em");
      this.legendText.appendChild(span);
      return span;
    };

    let line = addLine();
    for(const word of text.split(" ")){
      const before = line.textContent;
      line.textContent = before ? `${before} ${word}` : word;
      if(before && line.getComputedTextLength() > available){
        line.textContent = before;
        line = addLine();
        line.textContent = word;
      }
    }

    // Growing upwards from the legend's line, which is near the bottom
    const lines = this.legendText.children.length;
    this.legendText.firstChild.setAttribute("dy", `${-1.2 * (lines - 1)}em`);
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
    if(!this.currentChannel){
      return null;
    }

    return this.monitorRects[this._shownMonitor(this.currentChannel)] || null;
  }


  /**
   * Returns the monitor the region of the edited lamp is shown on: a lamp whose monitor isn't
   * captured shows its region on the first one, to be dragged onto a captured monitor
   */
  _shownMonitor(channel)
  {
    return channel.monitor === null ? 0 : channel.monitor;
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

      // Grid of the subsample the regions are averaged on, when it isn't too dense to see: one
      // repeated pattern instead of a line per row and column, cheaper to draw
      if(rect.width / rect.subsampleWidth >= 2){
        let cellWidth = rect.width / rect.subsampleWidth;
        let cellHeight = rect.height / rect.subsampleHeight;

        let pattern = document.createElementNS(svg, "pattern");
        pattern.id = `monitorGrid${i}`;
        pattern.setAttribute("patternUnits", "userSpaceOnUse");
        pattern.setAttribute("width", cellWidth);
        pattern.setAttribute("height", cellHeight);

        let cell = document.createElementNS(svg, "path");
        cell.setAttribute("d", `M ${cellWidth} 0 L 0 0 0 ${cellHeight}`);
        cell.classList.add("gridLine");
        pattern.appendChild(cell);

        let defs = document.createElementNS(svg, "defs");
        defs.appendChild(pattern);
        group.appendChild(defs);

        let grid = document.createElementNS(svg, "rect");
        grid.setAttribute("width", "100%");
        grid.setAttribute("height", "100%");
        grid.setAttribute("fill", `url(#${pattern.id})`);
        group.appendChild(grid);
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

      // Set here, once the language is known
      this.regionNode.setAttribute("aria-label", t(ScreenWidget.RegionKeysLabel));
      this.gammaCursor.handleNode.setAttribute("aria-label", t(ScreenWidget.GammaKeysLabel));
    }
  }


  /**
   * Arrow keys move the region by one subsample cell, with Shift they move its bottom right
   * corner, with Alt left and right they put it on the previous or next monitor
   */
  _onRegionKey(event)
  {
    const direction = {ArrowLeft : [-1, 0], ArrowRight : [1, 0], ArrowUp : [0, -1], ArrowDown : [0, 1]}[event.key];
    if(!direction || !this.currentChannel || !this.currentChannel.uvs){
      return;
    }

    event.preventDefault();

    const channelId = this.currentChannel.channelId;
    const index = this._shownMonitor(this.currentChannel);
    const uvs = this.currentChannel.uvs;

    if(event.altKey){
      const target = index + direction[0];
      if(direction[0] != 0 && target >= 0 && target < this.monitorRects.length){
        this.webApp.moveChannelRegion(channelId, target, uvs);
      }
      return;
    }

    const monitor = this.monitorRects[index];
    if(!monitor){
      return;
    }

    const cellWidth = 1 / monitor.subsampleWidth;
    const cellHeight = 1 / monitor.subsampleHeight;
    let a = {x : uvs.uvA.x, y : uvs.uvA.y};
    let b = {x : uvs.uvB.x, y : uvs.uvB.y};

    if(event.shiftKey){
      // At least one cell
      b.x = Utils.clamp(b.x + direction[0] * cellWidth, Math.min(a.x + cellWidth, 1), 1);
      b.y = Utils.clamp(b.y + direction[1] * cellHeight, Math.min(a.y + cellHeight, 1), 1);
    }
    else{
      const width = b.x - a.x;
      const height = b.y - a.y;
      a.x = Utils.clamp(a.x + direction[0] * cellWidth, 0, 1 - width);
      a.y = Utils.clamp(a.y + direction[1] * cellHeight, 0, 1 - height);
      b = {x : a.x + width, y : a.y + height};
    }

    // On the cell grid: repeated steps would otherwise add up rounding errors
    const snap = (point) => ({x : Math.round(point.x * monitor.subsampleWidth) / monitor.subsampleWidth, y : Math.round(point.y * monitor.subsampleHeight) / monitor.subsampleHeight});
    this.webApp.moveChannelRegion(channelId, index, {uvA : snap(a), uvB : snap(b)});
  }


  // Left and right (or down and up) arrows move the gamma cursor by one of its steps
  _onGammaKey(event)
  {
    const step = {ArrowLeft : -0.2, ArrowDown : -0.2, ArrowRight : 0.2, ArrowUp : 0.2}[event.key];
    if(step === undefined || !this.currentChannel){
      return;
    }

    event.preventDefault();
    this.gammaCursor.affectFactor(Utils.clamp(Utils.truncate(this.currentChannel.gammaFactor + step, 1), -1, 1));
  }


  /**
   * Fits a label into a box around its position: wrapped into lines, then made smaller down to
   * 8 px, and only then cut with an ellipsis. Narrow regions would otherwise let names overlap.
   */
  static fitLabel(textNode, text, maxWidth, maxHeight)
  {
    const LineHeight = 1.2;
    const x = textNode.getAttribute("x");

    const layout = (fontSize) => {
      textNode.style.fontSize = `${fontSize}px`;
      textNode.textContent = "";
      const lines = [];
      const addLine = () => {
        const span = document.createElementNS("http://www.w3.org/2000/svg", "tspan");
        span.setAttribute("x", x);
        textNode.appendChild(span);
        lines.push(span);
        return span;
      };

      let line = addLine();
      for(const word of text.split(" ")){
        const before = line.textContent;
        line.textContent = before ? `${before} ${word}` : word;
        if(before && line.getComputedTextLength() > maxWidth){
          line.textContent = before;
          line = addLine();
          line.textContent = word;
        }
      }

      const fits = lines.length * fontSize * LineHeight <= maxHeight && lines.every(span => span.getComputedTextLength() <= maxWidth);
      return {lines, fits};
    };

    let fontSize = 13;
    let result = layout(fontSize);
    while(!result.fits && fontSize > 8){
      fontSize--;
      result = layout(fontSize);
    }

    if(!result.fits){
      // As many lines as the height takes, the last one marked as cut, each one cut to the width
      const maxLines = Math.max(1, Math.floor(maxHeight / (fontSize * LineHeight)));
      if(result.lines.length > maxLines){
        while(result.lines.length > maxLines){
          result.lines.pop().remove();
        }
        result.lines[maxLines - 1].textContent += "…";
      }

      for(const span of result.lines){
        let content = span.textContent;
        while(content.length > 1 && span.getComputedTextLength() > maxWidth){
          content = content.replace(/…$/, "").slice(0, -1).trimEnd();
          span.textContent = `${content}…`;
        }
      }
    }

    // Centered on the label's position
    result.lines.forEach((span, index) => {
      span.setAttribute("dy", index == 0 ? `${-(result.lines.length - 1) * LineHeight / 2}em` : `${LineHeight}em`);
    });
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

      textNode.setAttribute("text-anchor", "middle");
      textNode.setAttribute("x", geometry.x + geometry.width / 2);
      textNode.setAttribute("y", geometry.y + geometry.height / 2);
      textNode.classList.add("previewLabel");

      rectNode.classList.add("previewRegion");

      let titleNode = document.createElementNS("http://www.w3.org/2000/svg", "title");
      titleNode.textContent = t("{lamp}: the screen region this lamp follows, filled with its current color. Select the lamp in the list to edit it", {lamp : activeChannel.formatMembers()});
      rectNode.appendChild(titleNode);
      rectNode.style.fill = this.regionColors[activeChannelId] || ScreenWidget.IdleRegionFill;
      this.previewRegions[activeChannelId] = rectNode;

      new Rectangle(rectNode).setGeometry(geometry.x, geometry.y, geometry.width, geometry.height);

      this.previewArea.appendChild(rectNode);
      this.previewArea.appendChild(textNode);

      // Measured once it's in the page
      ScreenWidget.fitLabel(textNode, activeChannel.formatMembers(), geometry.width * 0.9, geometry.height * 0.9);
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
    let geometry = this._regionGeometry(this.currentChannel ? this._shownMonitor(this.currentChannel) : null, uvs);
    if(!geometry){
      this.showWidgets(false);
      return;
    }

    // Without its monitor, the region can only be moved onto a captured one
    let missing = this.currentChannel.monitor === null;
    this.uvAreaNode.classList.toggle("missing", missing);
    if(missing){
      for(let handle of Object.values(this.handles)){
        handle.handleNode.style.display = "none";
      }
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

    // The name above the middle, the size below: each in its half of the region
    ScreenWidget.fitLabel(this.svgChannelMembersNamesNode, this.currentChannel.formatMembers(), geometry.width * 0.9, geometry.height * 0.42);
    ScreenWidget.fitLabel(this.svgChannelUVSizeNode, `${propWidth}% x ${propHeight}%`, geometry.width * 0.9, geometry.height * 0.42);
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

    let geometry = this._regionGeometry(this._shownMonitor(this.currentChannel), this.currentChannel.uvs);
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

    // On a coarser grid a small region could snap to nothing: keep at least one cell
    for(let [axis, steps] of [["x", target.subsampleWidth], ["y", target.subsampleHeight]]){
      if(uvs.uvB[axis] - uvs.uvA[axis] < 1 / steps){
        uvs.uvB[axis] = Math.min(uvs.uvA[axis] + 1 / steps, 1);
        uvs.uvA[axis] = uvs.uvB[axis] - 1 / steps;
      }
    }

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
ScreenWidget.RegionKeysLabel = "Screen region of the lamp. Arrow keys move it, with Shift they resize it, with Alt left and right they put it on the previous or next monitor";
ScreenWidget.GammaKeysLabel = "Gamma correction of the lamp. The arrow keys change it";
