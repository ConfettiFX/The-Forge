// Write your JavaScript code.

$(function () {

	/** @type {HTMLSelectElement} */
	var selectOption = document.getElementById('selectShader');


	var includeHeader = document.getElementById('IncludeHeader');

	var allShaders = window.AllShaders;	

	//var loading_bar = document.getElementById('output-loading');

	for (var kind of allShaders) {
		selectOption.options.add(new Option(kind, kind));
	}
	var CurrentFileName;
	var CurrentLanguage;

	//var IncludeFilesContents = ["", "", "", "", "", "", "", "",	"", "", "", "", "", "", "", "",];
	var includeFileCounter = 0;

	const codeMirrorTheme = "darcula";		
		const header = "/* Write your header comments here */\n";
	const defaultShader = "/* Example Source HLSL Fragment Shader */\n\nstruct VSOutput {\n\tfloat4 Position : SV_POSITION;\n    float4 Color : COLOR;\n};\n\nfloat4 main(VSOutput input) : SV_TARGET\n{\n    float4 Value;\n#if HLSL\n    Value = float4(1.0, 0.0, 0.0, 1.0);\n#elif GLSL\n    Value = float4(0.0, 1.0, 0.0, 1.0);\n#elif MSL\n    Value = float4(0.0, 0.0, 1.0, 1.0);\n#endif\n    return Value;\n}";
	
	var codeEditor0 = CodeMirror.fromTextArea(document.getElementById('TextArea'), {
		mode: "x-shader/hlsl",
		theme: codeMirrorTheme,
		lineNumbers: true,
		matchBrackets: true,
		styleActiveLine: true,
		indentUnit: 4
	});

	function init() {

		codeEditor0.setSize("100%", 200);
		codeEditor0.setValue(header);

		//startLoading();
	}

	init();
	

	var codeEditor1 = CodeMirror.fromTextArea(document.getElementById('TextArea1'), {
		mode: "x-shader/hlsl",
		theme: codeMirrorTheme,
		lineNumbers: true,
		matchBrackets: true,
		styleActiveLine: true,
		indentUnit: 4
	});

	var codeEditor2 = CodeMirror.fromTextArea(document.getElementById('TextArea2'), {
		mode: "x-shader/hlsl",
		theme: codeMirrorTheme,
		lineNumbers: true,
		matchBrackets: true,
		styleActiveLine: true,
		indentUnit: 4
	});

	


	var InputFile1 = document.getElementById("File1");
	var File1Label = document.getElementById("File1Label");

	
	var InputIncludeFile = document.getElementById("IncludeFile1");




	function getSelectedShader() {


		var result = allShaders.find(x => x === selectOption.selectedOptions[0].value);

		return result;
	}

	function createJsonRequestObject() {
		var jsonObject = {
			shader: getSelectedShader()
		};

		return jsonObject;
	}

	codeEditor1.setValue(defaultShader);
	setselectShader(1);

	function setselectShader(index) {

		var jsonObject = createJsonRequestObject();

		$.ajax({
			url: window.CompileUrl,
			type: "POST",
			data: JSON.stringify(jsonObject),
			contentType: "application/json; charset=utf-8",
			dataType: "json",
			error: function (response) {

				alert("Unexpected error");
			},
			success: function (responses) {


				var selectedStep = index;
				selectOption.selectedIndex = selectedStep;
			}
		});
	}


	

	

	function readURL(input) {

		//console.log("Read File...");

		const temporaryFileReader = new FileReader();

		return new Promise((resolve, reject) => {
			temporaryFileReader.onerror = () => {
				temporaryFileReader.abort();
				reject(new DOMException("Problem parsing input file."));
			};

			temporaryFileReader.onload = () => {
				resolve(temporaryFileReader.result);
				
			};

			temporaryFileReader.readAsText(input.files[0]);
		});
	}


	IncludeFile1.onchange = () => {

		var Result = readURL(IncludeFile1);

		Result.then(function (result) {

			
			var fileName = IncludeFile1.files[0];			

			//if it is same file name, ignore to add it
			var bDuplicated = false;
			for (var i = 0; i < includeHeader.options.length; i++)
			{
				if (includeHeader.options[i].value === fileName.name)
				{
					bDuplicated = true;
					break;
				}
			}

			if (!bDuplicated)
			includeHeader.options.add(new Option(fileName.name, fileName.name));

			
			var url = "/Home/CreateIncludeFile";

			var model = { FileName: fileName.name, FileContents: result};

			$.post(url, model, function (result)
			{
				finishLoading();

				//to reset file input event
				IncludeFile1.value = "";
			});
			

			//IncludeFilesContents[includeFileCounter++] = result;

		});
	};


	InputFile1.onchange = () => {

		var Result = readURL(InputFile1);

		Result.then(function (result) {

			//console.log("Get File!");

			//var name = document.getElementById("File1");
			var fileName = InputFile1.files[0];
			CurrentFileName = fileName.name;
			//if it is known extenions
			if (fileName !== undefined) {

				File1Label.textContent = fileName.name;

				var ext = fileName.name.split('.').pop();

				//console.log("ext : " + ext);

				switch (ext) {
					case "vert":
					case "src_vert":
						setselectShader(0);
						break;
					case "frag":
					case "src_frag":
						setselectShader(1);
						break;
					case "tesc":
					case "src_tesc":
						setselectShader(2);
						break;
					case "tese":
					case "src_tese":
						setselectShader(3);
						break;
					case "geom":
					case "src_geom":
						setselectShader(4);
						break;
					case "comp":
					case "src_comp":
						setselectShader(5);
						break;
					default:

						window.alert("Unknowned extension" + "\"" + ext + "\"" + " is founded! Please, select the right shader type before translate it.");
						break;
				}
			}
			else {
				window.alert("Error!");
			}

			codeEditor1.setValue(result);

			//to reset file input event
			InputFile1.value = "";

		});
	};	


	function GetErrorShader(param) {

		var splitted = param.split(')');

		if (splitted[0] === "error") {

			var model = { FileContents: codeEditor1.getValue() };
			var url = "/Home/SaveFeedbackFromClient";			
			$.post(url, model, function (result) {

			});
		}

	}

	function download(filename, text) {
		var element = document.createElement('a');
		element.setAttribute('href', 'data:text/plain;charset=utf-8,' + encodeURIComponent(text));
		element.setAttribute('download', filename);

		element.style.display = 'none';
		document.body.appendChild(element);

		element.click();

		document.body.removeChild(element);
	}

	$("#DownloadShader").click(function () {

		if (codeEditor2.getValue() === "") {

			window.alert("There is no output!");
			return;

		}

		var lastIndex;
		var newName;

		if (CurrentFileName === undefined || CurrentFileName === "") {
			newName = "output";
		}
		else {			
			lastIndex = CurrentFileName.lastIndexOf('.');
			newName = CurrentFileName.substr(0, lastIndex);
		}
		

		if (getSelectedShader() === "Vertex") {
			newName = newName + ".vert";
		}
		else if (getSelectedShader() === "Fragment") {
			newName = newName + ".frag";
		}
		else if (getSelectedShader() === "Hull") {
			newName = newName + ".tesc";
		}
		else if (getSelectedShader() === "Domain") {
			newName = newName + ".tese";
		}
		else if (getSelectedShader() === "Geometry") {
			newName = newName + ".geom";
		}
		else if (getSelectedShader() === "Compute") {
			newName = newName + ".comp";
		}

		if (CurrentLanguage === "MSL")
			newName += ".metal";

		download(newName, codeEditor2.getValue());
	})

	function startLoading() {
		//modal.style.display = "block";
		//$("#output-loading").show();
		$(".progress-bar").css("width", 100 + "%");
		//loading_bar
	}
	

	function finishLoading() {
		//$("#loading-container").hide();	
		//modal.style.display = "none";
		$(".progress-bar").css("width", 0 + "%");
		//loading_bar.setAttribute("aria-valuenow", "0"); 
	}

	function HomeParse(fileName, fileContents, entryName, shaderString, languageString)
	{
		if (fileName === undefined) {

			var model = { FileName: "NoFile", FileContents: fileContents, EntryName: entryName, Shader: shaderString, Language: languageString, Result: "" };

			startLoading();

			$.ajax({
				url: window.ParseUrl,
				type: "POST",
				data: JSON.stringify(model),
				contentType: "application/json; charset=utf-8",
				dataType: "json",
				error: function (response) {

					finishLoading();
					alert("Unexpected error");
				},
				success: function (responses) {

					finishLoading();
					codeEditor2.setValue(codeEditor0.getValue() + responses.result);
					GetErrorShader(responses.result);
				}
			});
		}
		else {
			model = { FileName: fileName, FileContents: fileContents, EntryName: entryName, Shader: shaderString, Language: languageString, Result: "" };

			startLoading();

			$.ajax({
				url: window.ParseUrl,
				type: "POST",
				data: JSON.stringify(model),
				contentType: "application/json; charset=utf-8",
				dataType: "json",
				error: function (response) {

					finishLoading();
					alert("Unexpected error");
				},
				success: function (responses) {

					finishLoading();
					codeEditor2.setValue(codeEditor0.getValue() + responses.result);
					GetErrorShader(responses.result);
				}
			});
		}

	}

	function Translate(param) {

		var name = document.getElementById("File1");
		var fileName = name.files[0];

		var shaderString = getSelectedShader();

		if (shaderString === "Vertex") {
			fileName = fileName + ".vert";
		}
		else if (shaderString === "Fragment") {
			fileName = fileName + ".frag";
		}
		else if (shaderString === "Hull") {
			fileName = fileName + ".tesc";
		}
		else if (shaderString === "Domain") {
			fileName = fileName + ".tese";
		}
		else if (shaderString === "Geometry") {
			fileName = fileName + ".geom";
		}
		else if (shaderString === "Compute") {
			fileName = fileName + ".comp";
		}

		var languageString = param;

		var fileContents = codeEditor1.getValue();//  InputShaderWindow.value;

		var entry = document.getElementById("Entry");
		var entryName = entry.value;

		if (entryName === "")
			entryName = "main";


		if (fileContents === undefined || fileContents === "") {

			window.alert("There is no input!");
			return;
		}		


		HomeParse(fileName, fileContents, entryName, shaderString, languageString);
		
	}

	
	$("#Translation").click(function () {

		Translate("HLSL");
		Translate("GLSL");

		if (getSelectedShader() === "Hull") {

			var vertexShaderInput = $(document.createElement("input"));
			vertexShaderInput.attr("type", "file");
			// add onchange handler if you wish to get the file :)

			window.alert("Please, open a vertex shader for generating Metal's hull shader!");

			vertexShaderInput.trigger("click"); // opening dialog

			vertexShaderInput.change(function () {

				var Result = readURL(this);

				Result.then(function (secondaryContents) {

					var name = document.getElementById("File1");
					var fileName = name.files[0];

					var shaderString = "Hull";
					var languageString = "MSL";

					var fileContents = codeEditor1.getValue();//  InputShaderWindow.value;

					var entry = document.getElementById("Entry");
					var entryName = entry.value;
					if (entryName === "")
						entryName = "main";


					if (fileContents === undefined || fileContents === "") {

						window.alert("There is no input!");
						return;
					}

					fileContents = secondaryContents + fileContents;


					//HomeParse(fileName, fileContents, entryName, shaderString, languageString);
					startLoading();

					var url = "/Home/LocalParse";

					if (fileName === undefined) {

						var model = { FileName: "NoFile", FileContents: fileContents, EntryName: entryName, Shader: shaderString, Language: languageString, Result: "" };

						$.post(url, model, function (result) {
							finishLoading();
							codeEditor2.setValue(codeEditor0.getValue() + result.result);
							GetErrorShader(result.result);

						});
					}
					else {
						model = { FileName: fileName.name, FileContents: fileContents, EntryName: entryName, Shader: shaderString, Language: languageString, Result: "" };

						$.post(url, model, function (result) {
							finishLoading();
							codeEditor2.setValue(codeEditor0.getValue() + result.result);
							GetErrorShader(result.result);
						});
					}

				});
			});
		}
		else
			Translate("MSL");

	})

	$("#ShowHLSL").click(function () {

		var url = "/Home/GetStoredResult";

		var model = { Language: "HLSL", Result: "" };

		$.post(url, model, function (result) {
			finishLoading();
			codeEditor2.setValue(codeEditor0.getValue() + result.result);
			GetErrorShader(result.result);

		});
	})

	$("#ShowGLSL").click(function () {

		var url = "/Home/GetStoredResult";

		var model = { Language: "GLSL", Result: "" };

		$.post(url, model, function (result) {
			finishLoading();
			codeEditor2.setValue(codeEditor0.getValue() + result.result);
			GetErrorShader(result.result);

		});
	})

	$("#ShowMSL").click(function () {

		var url = "/Home/GetStoredResult";

		var model = { Language: "MSL", Result: "" };

		$.post(url, model, function (result) {
			finishLoading();
			codeEditor2.setValue(codeEditor0.getValue() + result.result);
			GetErrorShader(result.result);

		});
	})



	$("#ToHLSL").click(function () {

		Translate("HLSL");
		CurrentLanguage = "HLSL";
	})

	$("#ToGLSL").click(function () {

		Translate("GLSL");
		CurrentLanguage = "GLSL";
	})

	$("#ToMSL").click(function () {

		if (getSelectedShader() === "Hull") {

			var vertexShaderInput = $(document.createElement("input"));
			vertexShaderInput.attr("type", "file");
			// add onchange handler if you wish to get the file :)

			window.alert("Please, open a vertex shader for generating Metal's hull shader!");

			vertexShaderInput.trigger("click"); // opening dialog

			vertexShaderInput.change(function () {

				var Result = readURL(this);

				Result.then(function (secondaryContents) {
					
					var name = document.getElementById("File1");
					var fileName = name.files[0];

					var shaderString = "Hull";
					var languageString = "MSL";

					var fileContents = codeEditor1.getValue();//  InputShaderWindow.value;

					var entry = document.getElementById("Entry");
					var entryName = entry.value;
					if (entryName === "")
						entryName = "main";


					if (fileContents === undefined || fileContents === "") {

						window.alert("There is no input!");
						return;
					}

					fileContents = secondaryContents + fileContents;


					//HomeParse(fileName, fileContents, entryName, shaderString, languageString);
					startLoading();
					
					var url = "/Home/LocalParse";

					if (fileName === undefined) {

						var model = { FileName: "NoFile", FileContents: fileContents, EntryName: entryName, Shader: shaderString, Language: languageString, Result: "" };

						$.post(url, model, function (result) {
							finishLoading();
							codeEditor2.setValue(codeEditor0.getValue() + result.result);
							GetErrorShader(result.result);

						});
					}
					else {
						model = { FileName: fileName.name, FileContents: fileContents, EntryName: entryName, Shader: shaderString, Language: languageString, Result: "" };

						$.post(url, model, function (result) {
							finishLoading();
							codeEditor2.setValue(codeEditor0.getValue() + result.result);
							GetErrorShader(result.result);
						});
					}
					
				});
			});
		}
		else
			Translate("MSL");

		CurrentLanguage = "MSL";
	})    
});





