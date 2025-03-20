const socket = new WebSocket(`ws://${location.hostname}:8080`);

socket.onerror = e => alert(JSON.stringify(e));

const container = document.querySelector("#c");
const form = document.querySelector("#f");

const pushMessage = (text: string) => {
    const el = document.createElement("div");
    el.classList.add("m");
    el.innerText = text;
    container?.appendChild(el);
}

// socket.addEventListener("open", () => console.log("Connected!"));
socket.addEventListener("message", async e => {
    console.log(e.data.constructor.name, typeof e.data, e.data instanceof String, e.data instanceof Blob, e.data instanceof ArrayBuffer, e.data);
    try {
        pushMessage(typeof e.data === "string" ? e.data
            : e.data instanceof Blob
            ? await e.data.text()
            : e.data instanceof ArrayBuffer
            ? new TextDecoder("utf-8").decode(e.data)
            : "Unknown encoding");
    } catch(_e) {
        pushMessage("Message failed to load");
    }
});

form?.addEventListener("submit", e => {
    e.preventDefault();
    const input = (e.target as HTMLFormElement)?.elements.namedItem("message") as HTMLInputElement;
    const text = input.value;
    socket.send(text);
    pushMessage(text);
    input.value = "";
});