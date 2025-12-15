export default class ImageHandler {
    constructor() {
        const self = this;
        this.maxWidth = 640;
        this.maxHeight = 480;
        this.images = [];
        this.lastImage = "";
        this.lastImageHasDiff = false;
        this.cropX = 0;
        this.cropY = 0;
        this.cropW = 0;
        this.cropH = 0;
        this.images = [];
        this.showCrop = false;

        this.originalWidth = this.maxWidth; // Image width
        this.originalHeight = this.maxHeight; // Image height
        this.newCanvasWidth = this.originalWidth;
        this.newCanvasHeight = this.originalHeight;
        //console.log("new canvas width:", this.newCanvasWidth, "new canvas height:", newCanvasHeight);
        this.imageWidth = this.cropW;
        this.imageHeight = this.cropH;
        this.newImageWidth = this.imageWidth;
        this.newImageHeight = this.imageHeight;
        //console.log("new image width:", newImageWidth, "new image height:", newImageHeight, "total pixels:", newImageWidth * newImageHeight);
    }

    updateCropParameters(layer) {
        console.log("updating crop parameters, layer: ", layer.x, layer.y, layer.width, layer.height);
        if (layer.x < 0) {
            this.cropX = 0;
        } else if (layer.x > (this.maxWidth - 10)) {
            this.cropX = this.maxWidth - 10;
        } else {
            this.cropX = Math.round(layer.x);
        }

        if (layer.y < 0) {
            this.cropY = 0;
        } else if (layer.y > (this.maxHeight - 10)) {
            this.cropY = this.maxHeight - 10;
        } else {
            this.cropY = Math.round(layer.y);
        }

        if (layer.width < 10) {
            this.cropW = 10;
        } else if ((layer.width + layer.x) > this.maxWidth) {
            this.cropW = this.maxWidth - layer.x;
        } else {
            this.cropW = Math.round(layer.width);
        }

        if (layer.height < 10) {
            this.cropH = 10;
        } else if ((layer.height + layer.y) > this.maxHeight) {
            this.cropH = this.maxHeight - layer.y;
        } else {
            this.cropH = Math.round(layer.height);
        }
        console.log("crop parameters: ", this.cropX, this.cropY, this.cropW, this.cropH);
    };
    loadImage(url = null) {
        this.showLoading();
        let imgUrl = this.lastImage;
        if (url) {
            imgUrl = url;
            this.lastImage = url;
        }
        $.ajax({
            url: imgUrl,
            method: 'GET',
            xhrFields: {
                responseType: 'blob'
            },
            success: (data) => {
                this.drawImage(data, '#canvas');
                this.showDone();
            },
            error: function (xhr, status, error) {
                console.error('Error fetching the raw image:', error);
                this.showLoadingFailed();
            }
        });
    };

    loadDiff(url) {
        var url_diff = url.slice(0, url.indexOf('.')) + '_c' + url.slice(url.indexOf('.'), url.length);

        $.ajax({
            url: url_diff,
            method: 'GET',
            xhrFields: {
                responseType: 'blob'
            },
            success: (data) => {
                this.drawImage(data, '#canvas');
                this.showDone();
            },
            error: (xhr, status, error) => {
                console.error("Error fetching diff image:", error);
                this.showLoadingFailed();
            }
        });
    };
    compareImages(data1, data2) {
        console.log("Comparing images");
        const rawData1 = new Uint8Array(data1);
        const rawData2 = new Uint8Array(data2);
        const diffData = new Uint8Array(rawData1.length);
        const average1 = rawData1.reduce((a, b) => a + b) / rawData1.length;
        const average2 = rawData2.reduce((a, b) => a + b) / rawData2.length;
        const scaleFactor = average1 / average2;
        for (let i = 0; i < rawData1.length; i++) {
            diffData[i] = Math.round(Math.abs(rawData1[i] - rawData2[i] * scaleFactor));
        }
    };
    getImages() {
        this.showLoading();
        console.log("Getting images");
        $.ajax({
            url: "/pics/pics.csv",
            method: 'GET',
            success: (data) => {
                //console.log("Images Data: \n", data);
                const rows = data.trim().split('\n');
                this.images = rows.map(row => {
                    const [filename, size, timestamp, change] = row.split(',');
                    return { filename, size, timestamp, change };
                });
                this.lastImage = `/pics/${this.images[this.images.length - 1].filename}`;
                this.lastImageHasDiff = this.images[this.images.length - 1].change === "0" ? false : true;
                if (this.lastImageHasDiff === true) {
                    this.loadDiff(this.lastImage);
                } else {
                    this.loadImage(this.lastImage);
                }
                $("#imageList").empty();
                for (let i = this.images.length - 1; i >= 0; i--) {
                    const image = this.images[i];
                    const timestamp = new Date(parseInt(image.timestamp) * 1000);
                    const dateString = timestamp.toLocaleString();
                    const path = `/pics/${image.filename}`;
                    const changed = image.change === "0" ? false : true;
                    const changeText = changed ? "change detected" : "no change";
                    const $listItem = $(`<li>${dateString}&nbsp;${changeText}</li>`);
                    $listItem.on('click', () => {
                        this.loadImage(path);
                    });
                    $("#imageList").append($listItem);
                }
            },
            error: (xhr, status, error) => {
                console.error("Error fetching images:", error);
                this.showLoadingFailed();
            }
        });
    };

    takePicture() {
        $.ajax({
            url: "/capture",
            type: "GET",
            success: (response) => {
                console.log("Picture taken successfully");
                this.getImages();
            },
            error: function (xhr, status, error) {
                console.log("Error taking picture:", error);
                this.showLoadingFailed();
            }
        });
    };

    drawDiff(data) {
        const rawData = new Uint8Array(data);
        const canvas = $('#canvas')[0];
        const ctx = canvas.getContext('2d');
        const imageDiff = ctx.createImageData(this.newImageWidth, this.newImageHeight);
        let diffIndex = 0;
        for (let xo = 0; xo < this.imageWidth; xo++) {
            for (let yo = 0; yo < this.imageHeight; yo++) {
                const originalIndex = yo * this.imageWidth + xo;
                const grayscale = rawData[originalIndex];
                for (let dx = 0; dx < this.scale; dx++) {
                    for (let dy = 0; dy < this.scale; dy++) {
                        const scaledX = (xo * this.scale + dx);
                        const scaledY = (yo * this.scale + dy);
                        const scaledIndex = (scaledY * this.newImageWidth + scaledX) * 4;
                        //imageDiff.data[scaledIndex + 1] = 0;
                        //imageDiff.data[scaledIndex + 2] = 0;
                        if (grayscale > $("#differenceThreshold").val()) {
                            imageDiff.data[scaledIndex] = 255;
                            imageDiff.data[scaledIndex + 3] = 255;
                            diffIndex++;
                        } else {
                            //imageDiff.data[scaledIndex] = 0;
                            imageDiff.data[scaledIndex + 3] = 0;
                        }
                        //console.log('Grayscale:', grayscale, 'Alpha:', imageDiff.data[scaledIndex + 3], "r", imageDiff.data[scaledIndex], "g", imageDiff.data[scaledIndex + 1], "b", imageDiff.data[scaledIndex + 2]);
                    }
                }
            }
        }
        const drawX = this.cropX * this.scale;
        const drawY = this.cropY * this.scale;
        //console.log('Drawing image on canvas at coordinates:', drawX, drawY);
        ctx.putImageData(imageDiff, drawX, drawY);
        const imageDiffUrl = canvas.toDataURL();
        ctx.clearRect(0, 0, canvas.width, canvas.height);
        $('#canvas').removeLayer('diffImage').drawLayers();
        $('#canvas').addLayer({
            type: 'image',
            name: 'diffImage',
            source: imageDiffUrl,
            x: 0,
            y: 0,
            fromCenter: false,
        }).drawLayers();
        this.showDone(diffIndex);
        console.log('Diff rendered on canvas');
    }

    drawImage(blob, canvas_name) {
        const canvas = $(canvas_name)[0];
        const ctx = canvas.getContext('2d');
        const image = new Image();

        // Create a URL for the Blob (this will allow the image to load)
        const imageURL = URL.createObjectURL(blob);

        image.onload = () => {
            // Set canvas size based on the image's original dimensions (or scale it)
            this.imageWidth = this.cropW;
            this.imageHeight = this.cropH;
            this.newImageWidth = this.imageWidth;
            this.newImageHeight = this.imageHeight;

            canvas.width = this.maxWidth;
            canvas.height = this.maxHeight;

            // Clear previous layers
            $(canvas_name).removeLayers();
            $(canvas_name).clearCanvas();

            // Draw black background
            $(canvas_name).addLayer({
                type: 'rectangle',
                fillStyle: 'black',
                name: 'background',
                x: 0,
                y: 0,
                width: this.newCanvasWidth,
                height: this.newCanvasHeight,
                fromCenter: false,
            }).drawLayers();

            // Draw the image onto the canvas
            const drawX = this.cropX;
            const drawY = this.cropY;
            // ctx.drawImage(image, 0, 0, this.imageWidth, this.imageHeight, drawX, drawY, this.newImageWidth, this.newImageHeight);
            ctx.drawImage(image, drawX, drawY, this.newImageWidth, this.newImageHeight);

            const imageDataUrl = canvas.toDataURL();

            // Add the image layer to the canvas
            $(canvas_name).addLayer({
                type: 'image',
                name: 'image',
                source: imageDataUrl,
                x: 0,
                y: 0,
                fromCenter: false,
            }).drawLayers();

            console.log('JPEG image rendered on canvas');
            if ($("#showcrop").is(":checked")) {
                this.showCropRectangle();
            }
        };

        image.src = imageURL;  // Start loading the image
    };

    showCropRectangle() {
        console.log("adding crop rectangle");
        if (this.cropX === 0 && this.cropY === 0 && this.cropW === 0 && this.cropH === 0) {
            console.log('Setting default crop parameters');
            this.cropW = 100;
            this.cropH = 100;
            this.cropX = 0;
            this.cropY = 0;
        }
        let cropRectangleWidth = this.cropW;
        let cropRectangleHeight = this.cropH;
        let cropRectangleX = this.cropX;
        let cropRectangleY = this.cropY;
        $('#canvas').addLayer({
            type: 'rectangle',
            name: 'cropRectangle',
            draggable: true,
            strokeStyle: '#ff0000',
            strokeWidth: 2,
            x: cropRectangleX,
            y: cropRectangleY,
            width: cropRectangleWidth,
            height: cropRectangleHeight,

            fromCenter: false,
            dragstop: (layer) => {
                this.updateCropParameters(layer);
            },
            handlestop: (layer) => {
                this.updateCropParameters(layer);
            },
            handlePlacement: 'both',
            handle: {
                type: 'rectangle',
                fillStyle: '#ff0000',
                width: 10,
                height: 10,
            },
            resizeFromCenter: false,
        }).drawLayers();
        // console.log("canvas size after showing rect:", $('#canvas').width(), $('#canvas').height());
        //console.log("canvas layers after showing rect:", $('#canvas').getLayers());
    };

    hideCropRectangle() {
        console.log("removing crop rectangle");
        $('#canvas').removeLayer('cropRectangle').drawLayers();
        // console.log("canvas size after removing rect:", $('#canvas').width(), $('#canvas').height());
        //  console.log("canvas layers after removing rect:", $('#canvas').getLayers());
    };

    saveCrop() {
        console.log("Before defaults:", this.cropX, this.cropY, this.cropW, this.cropH);
        console.log("Defaults:", this.maxWidth, this.maxHeight);
        if (this.cropX == undefined) {
            this.cropX = 0;
        }
        if (this.cropY == undefined) {
            this.cropY = 0;
        }
        if (this.cropW == undefined) {
            this.cropW = this.maxWidth;
        }
        if (this.cropH == undefined) {
            this.cropH = this.maxHeight;
        }
        console.log("saving crop, x:" + this.cropX + " y:" + this.cropY + " w:" + this.cropW + " h:" + this.cropH)
        $.ajax({
            url: "/savecrop",
            type: "GET",
            data: {
                cropX: this.cropX,
                cropY: this.cropY,
                cropWidth: this.cropW,
                cropHeight: this.cropH,
            },
            success: (data) => {
                console.log("Crop saved, taking first picture");
                this.takePicture();

            },
        });
    };
    showLoading() {
        $('#imageStatus').text("loading...");
    };
    showDone() {
        $('#imageStatus').text("done");
    };
    showLoadingFailed() {
        $('#imageStatus').text("loading failed");
    };
    resetMotion() {
        $.ajax({
            url: "/resetmotion",
            type: "GET",
            success: (data) => {
                console.log("Motion reset");
            },
        });
    }
}