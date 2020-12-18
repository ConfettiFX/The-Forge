#version 100
precision mediump float;
precision mediump int;

/*
 * Copyright (c) 2018-2020 The Forge Interactive Inc.
 * 
 * This file is part of The-Forge
 * (see https://github.com/ConfettiFX/The-Forge).
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
*/

// Copyright 2016 Intel Corporation All Rights Reserved
// 
// Intel makes no representations about the suitability of this software for any purpose.
// THIS SOFTWARE IS PROVIDED ""AS IS."" INTEL SPECIFICALLY DISCLAIMS ALL WARRANTIES,
// EXPRESS OR IMPLIED, AND ALL LIABILITY, INCLUDING CONSEQUENTIAL AND OTHER INDIRECT DAMAGES,
// FOR THE USE OF THIS SOFTWARE, INCLUDING LIABILITY FOR INFRINGEMENT OF ANY PROPRIETARY
// RIGHTS, AND INCLUDING THE WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
// Intel does not assume any responsibility for any errors which may appear in this software
// nor any responsibility to update it.

varying vec4 texcoord;

uniform sampler2D  RightText;
uniform sampler2D  LeftText;
uniform sampler2D  TopText;
uniform sampler2D  BotText;
uniform sampler2D  FrontText;
uniform sampler2D  BackText;

void main()
{
  vec2 newtextcoord;
  int side = int(texcoord.w);
  if(side==1)
  {
      newtextcoord = (texcoord.zy)/20.0 + vec2(0.5);
      newtextcoord = vec2(1.0 - newtextcoord.x, 1.0 - newtextcoord.y);
      gl_FragColor =  texture2D(RightText, newtextcoord);
  }
  else if(side==2)
  {
      newtextcoord = (texcoord.zy)/20.0 + vec2(0.5);
      newtextcoord = vec2(newtextcoord.x, 1.0 -newtextcoord.y);
      gl_FragColor =  texture2D(LeftText, newtextcoord);
  }
  else if(side==3)
  {
       gl_FragColor =  texture2D(TopText, (texcoord.xz)/20.0 + vec2(0.5));
  }
  else if(side == 4)
  {
       newtextcoord = (texcoord.xz)/20.0 + vec2(0.5);
       newtextcoord = vec2(newtextcoord.x, 1.0 -newtextcoord.y);
       gl_FragColor =  texture2D(BotText, newtextcoord);
  }
  else if(side==5)
  { 
       newtextcoord = (texcoord.xy)/20.0 + vec2(0.5);
       newtextcoord = vec2(newtextcoord.x, 1.0 - newtextcoord.y);
       gl_FragColor = texture2D(FrontText, newtextcoord);
       
  }
  else if(side==6)
  {
       newtextcoord = (texcoord.xy)/20.0 + vec2(0.5);
       newtextcoord = vec2(1.0 - newtextcoord.x, 1.0 - newtextcoord.y);
       gl_FragColor = texture2D(BackText, newtextcoord);
  }
}
