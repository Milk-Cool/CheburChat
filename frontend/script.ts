import errors from "./errors";
import resizeImage from "./image";
import { parsePacketPart, splitPacket } from "./packets";

const socket = new WebSocket(`ws://${location.hostname}:8080`);

socket.onerror = e => alert(JSON.stringify(e));

const container = document.querySelector("#c");
const form = document.querySelector("#f");
const settings = document.querySelector("#e");

document.querySelector("#s")?.addEventListener("click", () => settings?.classList.toggle("show"));

// TODO: scrolling on new messages when at the bottom

// https://stackoverflow.com/a/66046176
async function bufferToBase64(buffer: ArrayBuffer | Uint8Array): Promise<string> {
    const base64url: string = await new Promise(resolve => {
        const reader = new FileReader();
        reader.onload = () => resolve(reader.result as string);
        reader.readAsDataURL(new Blob([buffer]));
    });
    // Yes, we remove the MIME type since it might not always match what we recieve
    return base64url.slice(base64url.indexOf(",") + 1);
}

const pushMessage = async (data: string | Uint8Array, mimeType = "") => {
    const el = document.createElement("div");
    el.classList.add("m");
    if(typeof data === "string" || mimeType === "" || mimeType === "text/plain") {
        try {
            el.innerText = data instanceof Uint8Array ? new TextDecoder("utf-8").decode(data) : data;
        } catch(_e) {
            el.innerText = errors.invalidBinaryData;
        }
    } else {
        const dataURL = `data:${mimeType};base64,${await bufferToBase64(data)}`;
        if(mimeType.split("/")[0] === "image") {
            const img = document.createElement("img");
            img.src = dataURL;
            el.appendChild(img);
        } else if(mimeType.split("/")[0] === "audio") {
            const audio = document.createElement("audio");
            audio.src = dataURL;
            audio.controls = true;
            el.appendChild(audio);
        } else if(mimeType.split("/")[0] === "video") {
            const video = document.createElement("video");
            video.src = dataURL;
            video.controls = true;
            el.appendChild(video);
        }
    }
    container?.prepend(el);
}

socket.addEventListener("message", async e => {
    try {
        if(typeof e.data === "string")
            return pushMessage(e.data);
        const packet = parsePacketPart(e.data instanceof Blob ? await e.data.arrayBuffer() : e.data);
        if(packet === null) return;
        if(typeof packet[1] === "string")
            return pushMessage(packet[1]);
        pushMessage(packet[1], packet[0]);
    } catch(_e) {
        pushMessage(errors.failedToLoad);
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
            const image = e.target?.result as string;
            if(!image) return;
            const bufImg = Uint8Array.from(atob(image.slice(image.indexOf(",") + 1)), c => c.charCodeAt(0));
            const mime = image.split(",")[0].split(";")[0].split(":")?.[1] || "image/png";
            const split = splitPacket(bufImg, mime);
            for(const part of split) socket.send(part);
            pushMessage(bufImg, mime);
            fileInput.value = "";
        };
        reader.readAsDataURL(fileInput.files[0]);
    }
});