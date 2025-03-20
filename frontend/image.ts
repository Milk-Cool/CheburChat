export default function resizeImage(dataURL, maxWidth, maxHeight, mime): Promise<string> {
    return new Promise((resolve, reject) => {
        const image = new Image();
        image.onload = () => {
            const canvas = document.createElement("canvas");
            const width = Math.min(maxWidth, image.naturalWidth);
            const height = Math.min(maxHeight, width * (image.naturalHeight / image.naturalWidth));
            canvas.width = width;
            canvas.height = height;
            const ctx = canvas.getContext("2d");
            if(!ctx) return reject();

            ctx.drawImage(image, 0, 0, width, height);
            resolve(canvas.toDataURL(mime));
        };
        image.onerror = e => reject(e);
        image.src = dataURL;
    });
}