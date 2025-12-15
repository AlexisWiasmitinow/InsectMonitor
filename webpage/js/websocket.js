export function connectWebSocket() {
    const maxTerminalEntries = 20;
    const hostname = window.location.hostname;
    const socket = new WebSocket(`ws://${hostname}/ws`);
    socket.onopen = function (e) {
        console.log("Socket connection established");
    };
    socket.onmessage = function (event) {
        if ($('.terminal').length > maxTerminalEntries) {
            $('.terminal').first().remove();
        }
        $("#terminal_output").append("<p class='terminal'>" + event.data + "</p>");
    };
    socket.onclose = function (event) {
        if (event.wasClean) {
            console.log(`[close] Connection closed cleanly, code=${event.code} reason=${event.reason}`);
        } else {
            console.log('[close] Connection died, retrying in 2 seconds');
            setTimeout(connectWebSocket(), 2000);
        }
    };

    socket.onerror = function (error) {
        console.log(`[error]`, error);
    };
}
