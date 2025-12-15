function dropHandler(ev) {
	$(".box").removeClass('dragging');
	$(".box:not(#drop_zone)").hide();
	console.log('File(s) dropped');
	var lang = $('html').attr('lang');
	//$('#update_text').text(translator.translate("update_text_upload")); //todo: fix this
	$('#update_text').text('Update started');
	ev.preventDefault();
	if (ev.dataTransfer.items) {
		[...ev.dataTransfer.items].forEach((item, i) => {
			if (item.kind === 'file') {
				const file = item.getAsFile();
				console.log(`File[${i}]: ${file.name}`);

				let url = "/update?filename=" + encodeURIComponent(file.name); // Match working request
				let needsReboot = true;
				if (file.name === "products.json" || file.name === "settings.json") {
					url = "/save";
					needsReboot = false;
				}

				const reader = new FileReader();
				reader.onload = function (event) {
					const fileData = event.target.result; // Raw binary data

					$.ajax({
						url: url,
						type: 'POST',
						data: fileData,
						processData: false,  // Prevent jQuery from processing data
						contentType: 'application/octet-stream', // Match working request
						success: function (response) {
							console.log('File uploaded successfully');
							$('#update_text').text(needsReboot ? 'Upload done, rebooting' : 'Upload successful');
							if (needsReboot) {
								setTimeout(() => location.reload(), 5000);
							} else {
								location.reload();
							}
						},
						error: function (xhr, status, error) {
							console.error('Upload failed:', error);
							$('#update_text').text('Upload Error');
							setTimeout(() => location.reload(), 5000);
						}
					});
				};
				reader.readAsArrayBuffer(file); // Read file as raw binary
			}
		});
	} else {
		[...ev.dataTransfer.files].forEach((file, i) => {
			console.log(`… file[${i}].name = ${file.name}`);
		});
	}
}
function dragOverHandler(ev) {
	ev.preventDefault();
	$(".box").addClass('dragging');
}
function startUpload() {
	var otafile = document.getElementById("otafile").files;

	if (otafile.length == 0) {
		alert("No file selected!");
	} else {
		document.getElementById("otafile").disabled = true;
		document.getElementById("upload").disabled = true;

		var file = otafile[0];
		var xhr = new XMLHttpRequest();
		xhr.onreadystatechange = function () {
			if (xhr.readyState == 4) {
				if (xhr.status == 200) {
					document.open();
					document.write(xhr.responseText);
					document.close();
				} else if (xhr.status == 0) {
					alert("Server closed the connection abruptly!");
					location.reload()
				} else {
					alert(xhr.status + " Error!\n" + xhr.responseText);
					location.reload()
				}
			}
		};

		xhr.upload.onprogress = function (e) {
			var progress = document.getElementById("progress");
			progress.textContent = "Progress: " + (e.loaded / e.total * 100).toFixed(0) + "%";
		};
		var url = "/update?filename=" + file.name;
		xhr.open("POST", url, true);
		xhr.send(file);
	}
}