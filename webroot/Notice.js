// Notice about AI assistance and the missing warranty, accepted before using the application.
// Its version is Core::NoticeVersion (Notice.hpp): increase it whenever this text changes, so
// users accept the new text.
class Notice
{
  /**
   * Fills a container with the notice and a checkbox to confirm it was read
   *
   * @param container Element to fill
   * @param onConfirmedChange Called with whether the checkbox is ticked
   */
  static render(container, onConfirmedChange)
  {
    let box = document.createElement("div");
    box.className = "noticeBox";

    let title = document.createElement("strong");
    title.textContent = t(Notice.Title);
    box.appendChild(title);

    for(let text of Notice.Paragraphs){
      let paragraph = document.createElement("p");
      paragraph.textContent = t(text);
      box.appendChild(paragraph);
    }

    let label = document.createElement("label");
    label.className = "noticeAcceptance";

    let checkbox = document.createElement("input");
    checkbox.type = "checkbox";
    checkbox.addEventListener("change", () => {onConfirmedChange(checkbox.checked);});

    let text = document.createElement("span");
    text.textContent = t("I have read and understood this notice.");

    label.replaceChildren(checkbox, text);
    container.replaceChildren(box, label);
  }
}


Notice.Title = "AI-assisted project, no warranty, no liability";

Notice.Paragraphs = [
  "Lightspill is developed with the help of AI: its changes since the fork of Huenicorn were largely written by an AI (Claude by Anthropic) and were tested only on a few setups.",
  "The maintainer is not responsible for the code, its correctness or its security, nor for the consequences of its use.",
  "Use it at your own risk. It is provided \"as is\", without any warranty, and to the extent permitted by law the maintainer accepts no liability for any damage or other consequences of its use. There is no obligation to provide support, maintenance, updates or fixes."
];
