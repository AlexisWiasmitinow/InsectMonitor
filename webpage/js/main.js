import ImageHandler from "./imagehandler.js";
//import { connectWebSocket } from './websocket.js';
import "./jquery-3.7.1.min.js";
import "./jcanvas.min.js"
import "./jcanvas-handles.min.js"
import Charts from "./charts.js"
const imageHandler = new ImageHandler();
const charts = new Charts();
var lastPhotoTS = 0;

$(function () {
    if (Notification.permission !== 'granted') {
        console.log("Requesting notification permission...");
        Notification.requestPermission();
    }
    loadSettings();
    setInterval(refreshStatus, 10000);

    $("#save_settings").click(function () {
        saveSettings();
    });
    $(".setio").click(function () {
        const id = $(this).attr("id");
        if (id === "test_maintains_mode") {
            maintainsSwitch(id);
        }
        setIO(id);
    });
    $("#crop").click(() => {
        console.log("Crop");
        imageHandler.saveCrop();

    });
    $('#save_network').on('click', function (event) {
        saveNetwork();
    });
    $('#terminal').click(function () {
        $('#terminal_output').toggle();
    });
    $('#terminal').click(function () {
        $('#terminal_output').toggle();
    });
    $('#take_picture').click(function () {
        imageHandler.takePicture();
    });
    $('#hide_json').click(function () {
        $("#imagelist").empty();
    })
    $("#loadImage").click(function () {
        imageHandler.loadImage();
    });
    $("#ap").click(function () {
        $("#ssid").val("");
        $("#password").val("");
    });
    $("#crop").hide();
    $("#resetcrop").hide();
    $("#showcrop").change(function () {
        if (this.checked) {
            imageHandler.showCropRectangle();
            $("#crop").show();
            $("#resetcrop").show();
        } else {
            imageHandler.hideCropRectangle();
            $("#crop").hide();
            $("#resetcrop").hide();
        }
    });
    $("#resetcrop").click(function () {
        imageHandler.cropX = 0;
        imageHandler.cropY = 0;
        imageHandler.cropW = imageHandler.maxWidth;
        imageHandler.cropH = imageHandler.maxHeight;
        imageHandler.saveCrop();
    });
    $("#client").click(function () {
        $("#ssid").val("");
        $("#password").val("");

        $("#wifi_mode_attention").text("Now ensure you are connected to the same wifi, then go to \"queeenbreeder.local\" (with correct name, if he changed it and a link)");
    });
    $("#ap").click(function () {
        $("#wifi_mode_attention").text("");
    });
    $("#reset_motion").click(function () {
        imageHandler.resetMotion();
    });
    $("#historic_update").click(function () {
        charts.updateDataList();
        charts.updateCharts();
    });
});

function loadSettings() {
    $.getJSON("/settings.json", function (data) {
        console.log("settings: ", data);
        $("#apiToken").val(data.apiToken);
        $("#deviceID").text("Your device ID: " + data.deviceId);
        $("#differenceThreshold").val(data.differenceThreshold);
        $("#checkMotionInterval").val(data.checkMotionInterval);
        $("#maxPhotos").val(data.maxPhotos);
        $("#temperatureTarget").val(data.temperatureTarget);
        $("#tempCheckInterval").val(data.tempCheckInterval);
        $("#humidityTarget").val(data.humidityTarget);
        $("#pumpMsPerH").val(data.pumpMsPerH);
        $("#humidityCheckInterval").val(data.humidityCheckInterval);
        $("#ssid").val(data.ssid);
        $("#password").val(data.password);
        imageHandler.cropX = data.cropX;
        imageHandler.cropY = data.cropY;
        imageHandler.cropW = data.cropWidth;
        imageHandler.cropH = data.cropHeight;
        let timeZone = data.timezone;
        getTZ(timeZone);
        if (data.wifiMode == "Client") {
            $("#client").prop("checked", true);
            $("#wifi_signal").show();
        } else {
            $("#ap").prop("checked", true);
            $("#wifi_signal").hide();
        }
    }).fail(function (jqXHR, textStatus, errorThrown) {
        console.log("Error getting settings: " + textStatus + ", " + errorThrown);
    });
}

function saveSettings() {
    $.ajax({
        url: "/updateSettings",
        type: "GET",
        data: {
            differenceThreshold: $("#differenceThreshold").val(),
            checkMotionInterval: $("#checkMotionInterval").val(),
            maxPhotos: $("#maxPhotos").val(),
            temperatureTarget: $("#temperatureTarget").val(),
            tempCheckInterval: $("#tempCheckInterval").val(),
            pumpMsPerH: $("#pumpMsPerH").val(),
            humidityTarget: $("#humidityTarget").val(),
            humidityCheckInterval: $("#humidityCheckInterval").val(),
            apiToken: $("#apiToken").val(),
        },
        success: function (response) {
            console.log("Success: ", response);
        },
        error: function (jqXHR, textStatus, errorThrown) {
            console.log("Error:", textStatus, errorThrown);
            console.log("Response Text:", jqXHR.responseText);
        }
    });
}

function refreshStatus() {
    let now = Math.floor(Date.now() / 1000);
    $.getJSON("/getStatus?" + now, function (data) {
        $("#currentTime").text(data.systemTime);
        console.log("motion detected? ", data.motionDetected);
        if (data.motionDetected === true) {
            notify("Motion detected", "We detected motion at " + data.systemTime);
        }
        if (data.motionDetected === true) {
            $("#motionStatus").text("we detected motion");
        } else {
            $("#motionStatus").text("no motion detected");
        }
        $('#temp').text(data.temperature.toFixed(1));
        if (data.temperature === -1000) {
            $('#temp_attention').show();
        } else {
            $('#temp_attention').hide();
        }
        $('#hum').text(data.humidity.toFixed(1));
        if (data.humidity === -1000) {
            $('#hum_attention').show();
        } else {
            $('#hum_attention').hide();
        }
        if (data.lastPhotoTS != lastPhotoTS) {
            imageHandler.getImages();
            lastPhotoTS = data.lastPhotoTS;
        }
        const timestamp = new Date(parseInt(data.lastMotionTS) * 1000);
        const dateString = 'Last Motion Detected: ' + timestamp.toLocaleString();
        $('#lastMotionTS').text(dateString);

        if (data.wifiSignal > -45) {
            $("#wifi_signal").text("Excellent (" + data.wifiSignal + "dBbm)").css('color', 'white').css('background-color', 'green');
        } else if (data.wifiSignal > -65) {
            $("#wifi_signal").text("Good (" + data.wifiSignal + "dBbm)").css('color', 'white').css('background-color', 'orange');
        } else if (data.wifiSignal > -85) {
            $("#wifi_signal").text("Fair (" + data.wifiSignal + "dBbm)").css('color', 'white').css('background-color', 'darkorange');
        } else {
            $("#wifi_signal").text("Poor (" + data.wifiSignal + "dBbm)").css('color', 'white').css('background-color', 'red');
        }
    }).fail(function (jqXHR, textStatus, errorThrown) {
        console.log("Error getting status: " + textStatus + ", " + errorThrown);
    });
}

function setIO(id) {
    $.ajax({
        url: "/setio",
        type: "GET",
        data: {
            io: id
        },
        success: function (data) {
            console.log("IO " + id + " set to " + data);
        }
    });
}

function getTZ(timeZone = 'UTC0') {
    $.ajax({
        url: '/gettz',
        method: 'GET',
        dataType: 'json',
        success: function (data) {
            // Populate the dropdown with the fetched timezones
            $('#timezone').empty();
            $.each(data, function (index, timezone) {
                let selected = false;
                if (timezone.posix === timeZone) {
                    selected = true;
                }
                $('#timezone').append(
                    $('<option>', {
                        value: timezone.posix,
                        text: timezone.name,
                        selected: selected
                    })
                );
            });
        },
        error: function (xhr, status, error) {
            console.error('Error fetching timezones:', error);
        }
    });
}

function saveNetwork() {
    const data = {
        ssid: $('#ssid').val(),
        password: $('#password').val(),
        wifiMode: $('input[name="mode"]:checked').val(),
        timezone: $('#timezone').val(),
    };

    $.ajax({
        url: '/savenetwork',
        type: 'GET',
        contentType: 'application/json',
        data: data,
        success: function (response) {
            console.log('data saved');
        },
        error: function (xhr, status, error) {
            console.log('error occured: ' + error);
        }
    });
}

function maintainsSwitch(id) {
    const button = document.getElementById(id);
    if (!button) return;
    const isActive = button.classList.toggle("active");
    button.textContent = isActive ? "Maintains mode ON" : "Maintains mode OFF";
}

function notify(title, message) {
    if (Notification.permission === 'granted') {
        new Notification(title, { body: message });
    } else if (Notification.permission !== 'denied') {
        Notification.requestPermission().then(permission => {
            if (permission === 'granted') {
                new Notification(title, { body: message });
            }
        });
    }
}
