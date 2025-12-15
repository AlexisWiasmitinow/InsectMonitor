import "./chart.umd.min.js";
import "./chartjs-adapter-luxon.umd.min.js";
import { CHART_COLORS, transparentize } from "./utils.js";

export default class Charts {
    constructor() {
        this.chartTemp = null;
        this.chartHum = null;

        this.updateDataList();
        $('#historic_data_selector').on('change', () => this.updateCharts());
    }
    updateDataList() {
        $.getJSON("/historicDataList", (data) => {
            $('#historic_data_selector').empty();

            // Populate temperature dropdown
            $.each(data.temperature, function (index, fileName) {
                const dateStr = fileName.match(/temp_(\d+)_(\d+)_(\d+)\.csv/);
                let readable = fileName;
                if (dateStr) {
                    const [_, year, month, day] = dateStr;
                    const date = new Date(year, month, day); // month is 0-based
                    readable = date.toLocaleDateString(undefined, {
                        year: 'numeric',
                        month: 'short',
                        day: 'numeric'
                    });
                }

                $('#historic_data_selector').append(
                    $('<option>', {
                        value: fileName,
                        text: readable
                    })
                );
            });

            $('#historic_data_selector option:last').prop('selected', true);

            this.updateCharts();
        });
    }

    updateTemperatureChart() {
        var ctx = document.getElementById('chartTemperature').getContext('2d');
        var tempFile = $('#historic_data_selector').val();
        if (tempFile == null) {
            console.log("No temperature file selected.");
            return;
        }
        const tempPath = `logs/${tempFile}`;
        let tempData = [];

        $.ajax({
            url: tempPath,
            type: "GET",
            success: (data) => {
                const rows = data.trim().split('\n');

                tempData = rows.map(row => {
                    const [timestamp, temperature, state] = row.split(',');
                    return {
                        timestamp: new Date(parseInt(timestamp) * 1000),  // Convert to JS Date object
                        temperature: parseFloat(temperature),  // Convert string to float
                        state: parseInt(state)
                    };
                });

                var timestamps = tempData.map(item => item.timestamp);
                var temperatures = tempData.map(item => item.temperature);
                var states = tempData.map(item => item.state);
                const targetTemperature = $("#temperatureTarget").val();

                var chartData = {
                    labels: timestamps,
                    datasets: [
                        {
                            label: 'Temperature (°C)',
                            data: temperatures,
                            borderColor: 'blue',
                            borderWidth: 2,
                            fill: false,
                            tension: 0.4,
                            yAxisID: 'y',
                            radius: 0,
                        },
                        {
                            label: 'Target Temperature (°C)',
                            data: [
                                { x: timestamps[0], y: targetTemperature },
                                { x: timestamps[timestamps.length - 1], y: targetTemperature }
                            ],
                            borderColor: 'green',
                            borderWidth: 1,
                            borderDash: [5, 5],  // Dashes for target line
                            fill: false,
                            yAxisID: 'y',  // Use the same y-axis for temperature
                        },
                        {
                            label: 'Heater State',
                            data: states,
                            borderColor: 'red',
                            borderWidth: 1,
                            fill: false,
                            yAxisID: 'y1',
                            stepped: true,
                            radius: 0,
                        },
                    ]
                };

                // Destroy existing chart if it exists
                if (this.chartTemp !== null) {
                    this.chartTemp.destroy();
                }

                // Create new Chart.js instance with temperature and humidity data
                this.chartTemp = new Chart(ctx, {
                    type: 'line',
                    data: chartData,
                    options: {
                        responsive: true,
                        scales: {
                            x: {
                                type: 'time',
                                time: {
                                    unit: 'minute'
                                }
                            },
                            y: {
                                beginAtZero: false,
                                suggestedMin: 10,
                                suggestedMax: 50,
                                position: 'left',
                                title: {
                                    display: true,
                                    text: 'Temperature (°C)'
                                },
                            },
                            y1: {
                                beginAtZero: true,
                                suggestedMin: 0,
                                suggestedMax: 1,
                                grid: {
                                    drawOnChartArea: false,
                                },
                                position: 'right',
                                ticks: {
                                    stepSize: 1,
                                    callback: function (value) {
                                        return value === 1 ? 'ON' : 'OFF';
                                    }
                                }
                            },
                        }
                    }
                });
            },
            error: function (jqXHR, textStatus, errorThrown) {
                console.log("Temperature Data Error:", textStatus, errorThrown);
                console.log("Response Text:", jqXHR.responseText);
            }
        });
    }

    updateHumidityChart() {
        var ctx = document.getElementById('chartHumidity').getContext('2d');
        var humFile = $('#historic_data_selector').val();
        if (humFile == null) {
            console.log("No humidity file selected.");
            return;
        }
        humFile = humFile.replace('temp', 'hum');
        const humPath = `logs/${humFile}`;
        let humData = [];

        $.ajax({
            url: humPath,
            type: "GET",
            success: (data) => {
                const rows = data.trim().split('\n');

                humData = rows.map(row => {
                    const [timestamp, humidity, state] = row.split(',');
                    return {
                        timestamp: new Date(parseInt(timestamp) * 1000),
                        humidity: parseFloat(humidity),
                        state: parseInt(state)
                    };
                });
                var timestamps = humData.map(item => item.timestamp);
                var humidities = humData.map(item => item.humidity);
                var states = humData.map(item => item.state);
                const targetHumidity = $("#humidityTarget").val();
                var chartData = {
                    labels: timestamps,
                    datasets: [
                        {
                            label: 'Humidity (%)',
                            data: humidities,
                            borderColor: 'green',
                            borderWidth: 2,
                            fill: false,
                            tension: 0.4,
                            yAxisID: 'y',
                            radius: 0,
                        },
                        {
                            label: 'Target Humidity (%)',
                            data: [
                                { x: timestamps[0], y: targetHumidity },
                                { x: timestamps[timestamps.length - 1], y: targetHumidity }
                            ],
                            borderColor: 'purple',
                            borderWidth: 1,
                            borderDash: [5, 5],  // Dashes for target line
                            fill: false,
                            yAxisID: 'y',  // Use the same y-axis for temperature
                        },
                        {
                            label: 'Pump time, ms',
                            data: states,
                            borderColor: 'blue',
                            borderWidth: 1,
                            fill: false,
                            yAxisID: 'y1',
                            stepped: true,
                            radius: 0,
                        },
                    ]
                };

                // Destroy existing chart if it exists
                if (this.chartHum !== null) {
                    this.chartHum.destroy();
                }

                // Create new Chart.js instance with temperature and humidity data
                this.chartHum = new Chart(ctx, {
                    type: 'line',
                    data: chartData,
                    options: {
                        responsive: true,
                        scales: {
                            x: {
                                type: 'time',
                                time: {
                                    unit: 'minute'
                                }
                            },
                            y: {
                                beginAtZero: false,
                                suggestedMin: 0,
                                suggestedMax: 100,
                                title: {
                                    display: true,
                                    text: 'Humidity (%)'
                                },
                            },
                            y1: {
                                beginAtZero: true,
                                suggestedMin: 0,
                                suggestedMax: 1,
                                grid: {
                                    drawOnChartArea: false,
                                },
                                position: 'right',
                                title: {
                                    display: true,
                                    text: 'Pump time, ms'
                                }
                                // ticks: {
                                //     stepSize: 1,
                                //     callback: function(value) {
                                //         return value === 1 ? 'Pump ON' : 'Pump OFF';
                                //     }
                                // }
                            },
                        }
                    }
                });
            },
            error: function (jqXHR, textStatus, errorThrown) {
                console.log("Humidity Data Error:", textStatus, errorThrown);
                console.log("Response Text:", jqXHR.responseText);
            }
        });
    }

    updateCharts() {
        this.updateHumidityChart();
        this.updateTemperatureChart();
    }
}