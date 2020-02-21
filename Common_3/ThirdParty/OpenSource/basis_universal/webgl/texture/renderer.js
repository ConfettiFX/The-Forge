/**
 * Constructs a renderer object.
 * @param {WebGLRenderingContext} gl The GL context.
 * @constructor
 */
var Renderer = function(gl) {
  /**
   * The GL context.
   * @type {WebGLRenderingContext}
   * @private
   */
  this.gl_ = gl;

  /**
   * The WebGLProgram.
   * @type {WebGLProgram}
   * @private
   */
  this.program_ = gl.createProgram();

  /**
   * @type {WebGLShader}
   * @private
   */
  this.vertexShader_ = this.compileShader_(
      Renderer.vertexShaderSource_, gl.VERTEX_SHADER);

  /**
   * @type {WebGLShader}
   * @private
   */
  this.fragmentShader_ = this.compileShader_(
      Renderer.fragmentShaderSource_, gl.FRAGMENT_SHADER);

  /**
   * Cached uniform locations.
   * @type {Object.<string, WebGLUniformLocation>}
   * @private
   */
  this.uniformLocations_ = {};

  /**
   * Cached attribute locations.
   * @type {Object.<string, WebGLActiveInfo>}
   * @private
   */
  this.attribLocations_ = {};

  /**
   * A vertex buffer containing a single quad with xy coordinates from [-1,-1]
   * to [1,1] and uv coordinates from [0,0] to [1,1].
   * @private
   */
  this.quadVertexBuffer_ = gl.createBuffer();
  gl.bindBuffer(gl.ARRAY_BUFFER, this.quadVertexBuffer_);
  var vertices = new Float32Array(
      [-1.0, -1.0, 0.0, 1.0,
       +1.0, -1.0, 1.0, 1.0,
       -1.0, +1.0, 0.0, 0.0,
        1.0, +1.0, 1.0, 0.0]);
  gl.bufferData(gl.ARRAY_BUFFER, vertices, gl.STATIC_DRAW);


  // init shaders

  gl.attachShader(this.program_, this.vertexShader_);
  gl.attachShader(this.program_, this.fragmentShader_);
  gl.bindAttribLocation(this.program_, 0, 'vert');
  gl.linkProgram(this.program_);
  gl.useProgram(this.program_);
  gl.enableVertexAttribArray(0);

  gl.enable(gl.DEPTH_TEST);
  gl.disable(gl.CULL_FACE);

  var count = gl.getProgramParameter(this.program_, gl.ACTIVE_UNIFORMS);
  for (var i = 0; i < /** @type {number} */(count); i++) {
    var info = gl.getActiveUniform(this.program_, i);
    var result = gl.getUniformLocation(this.program_, info.name);
    this.uniformLocations_[info.name] = result;
  }

  count = gl.getProgramParameter(this.program_, gl.ACTIVE_ATTRIBUTES);
  for (var i = 0; i < /** @type {number} */(count); i++) {
    var info = gl.getActiveAttrib(this.program_, i);
    var result = gl.getAttribLocation(this.program_, info.name);
    this.attribLocations_[info.name] = result;
  }
};


Renderer.prototype.finishInit = function() {
  this.draw();
};


Renderer.prototype.createDxtTexture = function(dxtData, width, height, format) {
  var gl = this.gl_;
  var tex = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.compressedTexImage2D(
      gl.TEXTURE_2D,
      0,
      format,
      width,
      height,
      0,
      dxtData);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  //gl.generateMipmap(gl.TEXTURE_2D)
  gl.bindTexture(gl.TEXTURE_2D, null);
  return tex;
};

Renderer.prototype.createCompressedTexture = function(data, width, height, format) {
  var gl = this.gl_;
  var tex = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.compressedTexImage2D(
      gl.TEXTURE_2D,
      0,
      format,
      width,
      height,
      0,
      data);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  //gl.generateMipmap(gl.TEXTURE_2D)
  gl.bindTexture(gl.TEXTURE_2D, null);
  return tex;
};


Renderer.prototype.createRgb565Texture = function(rgb565Data, width, height) {
  var gl = this.gl_;
  var tex = gl.createTexture();
  gl.bindTexture(gl.TEXTURE_2D, tex);
  gl.texImage2D(
    gl.TEXTURE_2D,
    0,
    gl.RGB,
    width,
    height,
    0,
    gl.RGB,
    gl.UNSIGNED_SHORT_5_6_5,
    rgb565Data);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MAG_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_MIN_FILTER, gl.LINEAR);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_S, gl.CLAMP_TO_EDGE);
  gl.texParameteri(gl.TEXTURE_2D, gl.TEXTURE_WRAP_T, gl.CLAMP_TO_EDGE);
  //gl.generateMipmap(gl.TEXTURE_2D)
  gl.bindTexture(gl.TEXTURE_2D, null);
  return tex;
};


Renderer.prototype.drawTexture = function(texture, width, height, mode) {
  var gl = this.gl_;
  // draw scene
  gl.clearColor(0, 0, 0, 1);
  gl.clearDepth(1.0);
  gl.viewport(0, 0, width, height);
  gl.clear(gl.COLOR_BUFFER_BIT | gl.DEPTH_BUFFER_BIT | gl.STENCIL_BUFFER_BIT);

  gl.activeTexture(gl.TEXTURE0);
  gl.bindTexture(gl.TEXTURE_2D, texture);
  gl.uniform1i(this.uniformLocations_.texSampler, 0);

  var x = 0.0;
  var y = 0.0;
  if (mode == 1)
  	x = 1.0;
  else if (mode == 2)
    y = 1.0;
	
  gl.uniform4f(this.uniformLocations_.control, x, y, 0.0, 0.0);

  gl.enableVertexAttribArray(this.attribLocations_.vert);
  gl.bindBuffer(gl.ARRAY_BUFFER, this.quadVertexBuffer_);
  gl.vertexAttribPointer(this.attribLocations_.vert, 4, gl.FLOAT,
      false, 0, 0);
  gl.drawArrays(gl.TRIANGLE_STRIP, 0, 4);
};


/**
 * Compiles a GLSL shader and returns a WebGLShader.
 * @param {string} shaderSource The shader source code string.
 * @param {number} type Either VERTEX_SHADER or FRAGMENT_SHADER.
 * @return {WebGLShader} The new WebGLShader.
 * @private
 */
Renderer.prototype.compileShader_ = function(shaderSource, type) {
  var gl = this.gl_;
  var shader = gl.createShader(type);
  gl.shaderSource(shader, shaderSource);
  gl.compileShader(shader);
  return shader;
};


/**
 * @type {string}
 * @private
 */
Renderer.vertexShaderSource_ = [
  'attribute vec4 vert;',
  'varying vec2 v_texCoord;',
  'void main() {',
  '  gl_Position = vec4(vert.xy, 0.0, 1.0);',
  '  v_texCoord = vert.zw;',
  '}'
  ].join('\n');


/**
 * @type {string}
 * @private '  gl_FragColor = texture2D(texSampler, v_texCoord);',
 */
Renderer.fragmentShaderSource_ = [
  'precision highp float;',
  'uniform sampler2D texSampler;',
  'uniform vec4 control;',
  'varying vec2 v_texCoord;',
  'void main() {',
  '  vec4 c;',
  '  c = texture2D(texSampler, v_texCoord);',
  '  if (control.x > 0.0)',
  '  {',
  '   	c.w = 1.0;',
  '  }',
  '	 else if (control.y > 0.0)',
  '	 {',
  '   	c.rgb = c.aaa; c.w = 1.0;',
  '  }',
  '  gl_FragColor = c;',
  '}'
  ].join('\n');
  
