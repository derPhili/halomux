log = console.log;


/**
 * Log page: the service's journal, updated every few seconds
 */
class LogPage
{
  constructor()
  {
    this.linesNode = document.getElementById("logLines");
    this.errorNode = document.getElementById("logsError");
    this.lastTime = null;

    this._refresh();
    setInterval(() => {this._refresh();}, LogPage.RefreshInterval);
  }


  _refresh()
  {
    RequestUtils.get("/api/logs")
    .then((data) => {
      this.errorNode.textContent = data.lines.length == 0 ? t("The journal can't be read, or holds no lines of Halomux yet.") : "";
      this._show(data.lines);
    })
    .catch((error) => {
      log(error);
      this.errorNode.textContent = t("Halomux doesn't answer.");
    });
  }


  _show(lines)
  {
    // Nothing new: the list and its scroll position stay
    const newest = lines.length > 0 ? `${lines[lines.length - 1].time}:${lines.length}:${lines[lines.length - 1].message}` : null;
    if(newest == this.lastTime){
      return;
    }
    this.lastTime = newest;

    // Following the newest lines, unless scrolled up to read older ones
    const scroller = document.scrollingElement;
    const atBottom = scroller.scrollHeight - scroller.scrollTop - scroller.clientHeight < 40;

    const items = lines.map((line) => {
      let item = document.createElement("li");
      item.className = `logLine ${line.level}`;

      let time = document.createElement("time");
      const date = new Date(line.time);
      time.dateTime = date.toISOString();
      time.textContent = date.toLocaleString(I18n.language, {dateStyle : "short", timeStyle : "medium"});

      let message = document.createElement("span");
      message.textContent = line.message;

      item.replaceChildren(time, message);
      return item;
    });
    this.linesNode.replaceChildren(...items);

    if(atBottom){
      scroller.scrollTop = scroller.scrollHeight;
    }
  }
}


// Between updates of the lines [ms]
LogPage.RefreshInterval = 3000;

I18n.init().then(() => {
  document.title = t(document.title);
  new LogPage();
});
