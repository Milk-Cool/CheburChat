import errors from "./errors";

const packets: Record<number, Uint8Array> = {};
const timeouts: Record<number, ReturnType<typeof setTimeout>> = {};

const keepPacketMS = 10000;
const maxPacketSize = 10000;

const errorMimeType = "text/plain";

export type ParsedPacket = [string, Uint8Array | string]

// Inner structure
// 1 byte - 0
// n bytes - MIME type as string
// 1 byte - 0
// k bytes - data

// Outer structure
// 1 byte - 0
// 4 bytes - random packet ID
// n >= 0 bytes - data
// ---
// if n = 0 then that packet was the last one

function parseWholePacket(data: Uint8Array): ParsedPacket {
    const header = data[0];
    if(header !== 0) {
        try {
            return [errorMimeType, new TextDecoder("utf-8").decode(data)];
        } catch(_e) {
            return [errorMimeType, errors.invalidBinaryData];
        }
    }
    let n = 1;
    while(data[n] != 0 && n < data.length) n++;
    let mimeType = "";
    try {
        mimeType = new TextDecoder("utf-8").decode(data.slice(1, n));
    } catch(_e) {
        return [errorMimeType, errors.invalidMimeType];
    }
    if(n === data.length)
        return [mimeType, new Uint8Array()];
    return [mimeType, data.slice(n + 1)];
}

export function parsePacketPart(data: Uint8Array | ArrayBuffer): ParsedPacket | null {
    if(data instanceof ArrayBuffer)
        data = new Uint8Array(data);
    if(!(data instanceof Uint8Array)) return null;
    if(data.length === 0) return null;
    const header = data[0];
    if(header !== 0) {
        try {
            return ["text/plain", new TextDecoder("utf-8").decode(data)];
        } catch(_e) {
            return [errorMimeType, errors.failedToLoad];
        }
    }
    if(data.length < 5) return null;
    let id = 0;
    for(let n = 1; n < 5; n++) {
        id <<= 8;
        id |= data[n];
    }
    if(data.length === 5) {
        if(!packets[id]) return null;
        const ret = parseWholePacket(packets[id]);
        clearTimeout(timeouts[id]);
        delete packets[id];
        delete timeouts[id];
        return ret;
    }
    if(timeouts[id]) clearTimeout(timeouts[id]);
    if(packets[id]) {
        const merged = new Uint8Array(packets[id].length + data.length - 5);
        merged.set(packets[id]);
        merged.set(data.slice(5), packets[id].length);
        packets[id] = merged;
    } else
        packets[id] = data.slice(5);
    timeouts[id] = setTimeout(() => {
        delete packets[id];
        delete timeouts[id];
    }, keepPacketMS);
    return null;
}

export function splitPacket(data: Uint8Array, mimeType: string): Uint8Array[] {
    let mimeTypeBin: Uint8Array;
    try {
        mimeTypeBin = new TextEncoder().encode(mimeType);
    } catch(_e) {
        return [];
    }
    const innerPacket = new Uint8Array(data.length + mimeTypeBin.length + 2);
    innerPacket[0] = 0;
    innerPacket.set(mimeTypeBin, 1);
    innerPacket[mimeTypeBin.length + 1] = 0;
    innerPacket.set(data, mimeTypeBin.length + 2);
    
    const randomID = Math.floor(Math.random() * 0xffffffff);
    let ret: Uint8Array[] = [];
    const idBin = new Uint8Array(5);
    idBin[0] = 0;
    idBin[1] = (randomID >> 24) & 0xff;
    idBin[2] = (randomID >> 16) & 0xff;
    idBin[3] = (randomID >> 8) & 0xff;
    idBin[4] = (randomID >> 0) & 0xff;
    for(let n = 0; n < innerPacket.length; n += maxPacketSize - 5) {
        const data = innerPacket.slice(n, Math.min(innerPacket.length, n + maxPacketSize - 5));
        const outerPacket = new Uint8Array(data.length + 5);
        outerPacket.set(idBin);
        outerPacket.set(data, 5);
        ret.push(outerPacket);
    }
    ret.push(idBin);
    return ret;
}