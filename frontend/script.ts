import resizeImage from "./image";

const maxWidth = 100;
const maxHeight = 100;
const imgMimeType = "image/png";

const socket = new WebSocket(`ws://${location.hostname}:8080`);

socket.onerror = e => alert(JSON.stringify(e));

const container = document.querySelector("#c");
const form = document.querySelector("#f");

// TODO: separate object (file?) with errors

const pushMessage = (data: string | Uint8Array | ArrayBuffer) => {
    if(data instanceof ArrayBuffer)
        data = new Uint8Array(data);
    const el = document.createElement("div");
    el.classList.add("m");
    if(typeof data === "string" && data.charCodeAt(0) !== 0) el.innerText = data;
    else {
        if(typeof data === "string") data = new Uint8Array([...data].map((c) => c.charCodeAt(0)));
        if(!(data instanceof Uint8Array)) return;
        const header = data[0];
        if(header !== 0) {
            try {
                el.innerText = new TextDecoder("utf-8").decode(data);
            } catch(_e) {
                el.innerText = "Invalid binary data";
            }
        } else {
            let n = 1;
            while(data[n] != 0) n++;
            const mimeType = new TextDecoder("utf-8").decode(data.slice(1, n));
            if(mimeType.split("/")[0] === "image") {
                const img = document.createElement("img");
                img.src = `data:${mimeType};base64,${btoa(String.fromCharCode(...data.slice(n + 1)))}`;
                el.appendChild(img);
            }
        }
    }
    container?.appendChild(el);
}

socket.addEventListener("message", async e => {
    try {
        pushMessage(typeof e.data === "string" ? e.data
            : e.data instanceof Blob
            ? await e.data.arrayBuffer()
            : e.data instanceof ArrayBuffer
            ? e.data
            : "Unknown encoding");
    } catch(_e) {
        pushMessage("Message failed to load");
    }
});

form?.addEventListener("submit", e => {
    e.preventDefault();
    const elements = (e.target as HTMLFormElement)?.elements;
    const textInput = elements.namedItem("message") as HTMLInputElement;
    const fileInput = elements.namedItem("file") as HTMLInputElement;
    if(textInput.value) {
        const text = textInput.value;
        socket.send(text);
        pushMessage(text);
        textInput.value = "";
    }
    if(fileInput.files && fileInput.files[0]) {
        const reader = new FileReader();
        reader.onload = async e => {
            const resized = await resizeImage(e.target?.result, maxWidth, maxHeight, imgMimeType);
            const bufImg = Uint8Array.from(atob(resized.slice(resized.indexOf(",") + 1)), c => c.charCodeAt(0));
            const bufMime = Uint8Array.from(imgMimeType, c => c.charCodeAt(0));
            const bufSep = Uint8Array.from([0]);
            const bufFinal = new Uint8Array(bufSep.length * 2 + bufMime.length + bufImg.length);
            bufFinal.set(bufSep, 0);
            bufFinal.set(bufMime, bufSep.length);
            bufFinal.set(bufSep, bufSep.length + bufMime.length);
            bufFinal.set(bufImg, bufSep.length * 2 + bufMime.length);
            socket.send(bufFinal);
            pushMessage(bufFinal);
            fileInput.value = "";
        };
        reader.readAsDataURL(fileInput.files[0]);
    }
});