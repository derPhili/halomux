log = console.log;

let webUI = null;

// Texts are translated before the UI is built
I18n.init().then(() => {
  webUI = new WebUI();
  webUI.initUI();
});
