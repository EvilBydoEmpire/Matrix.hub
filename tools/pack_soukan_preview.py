"""Encode lossless source atlas for S3 and flash-constrained WROOM targets."""
import io,json
from pathlib import Path
from PIL import Image
from profile_pattern_library import ASSET,HEADER,write_header
root=Path(__file__).resolve().parent.parent

def pack():
 source=Image.open(ASSET).convert('RGB');frames=source.width//28;count=source.height//28
 assert source.size==(28*frames,28*count)
 profiles={};headers={}
 for name,size,budget in (('hub75',28,500000),('ws2812',16,215000)):
  if size==28:atlas=source
  else:
   atlas=Image.new('RGB',(size*frames,size*count))
   for y in range(count):
    for x in range(frames):
     atlas.paste(source.crop((x*28,y*28,(x+1)*28,(y+1)*28)).resize((size,size),Image.Resampling.BOX),(x*size,y*size))
  for colors in (256,224,192,160,128,96,64):
   indexed=atlas.quantize(colors=colors,method=Image.Quantize.MEDIANCUT,dither=Image.Dither.NONE)
   stream=io.BytesIO();indexed.save(stream,format='PNG',optimize=True);data=stream.getvalue()
   if len(data)<=budget:break
  assert len(data)<=budget,(name,len(data))
  write_header(data,size,size,frames,count,67);headers[name]=HEADER.read_text()
  (root/f'assets/pattern-preview-soukan-{name}.png').write_bytes(data)
  error=sum(abs(a-b) for a,b in zip(atlas.tobytes(),indexed.convert('RGB').tobytes()))/(atlas.width*atlas.height*3)
  profiles[name]={'tileSize':size,'frames':frames,'patterns':count,'intervalMs':67,'colors':colors,'bytes':len(data),'meanAbsoluteRgb8Error':round(error,3)}
 HEADER.write_text('#pragma once\n// Same captured animation frames; target-specific thumbnail budget.\n#if defined(MATRIX_PROFILE_WS2812)\n'+headers['ws2812']+'\n#else\n'+headers['hub75']+'\n#endif\n')
 (root/'docs/soukan-preview-encoding.json').write_text(json.dumps(profiles,indent=2)+'\n')
 print(json.dumps(profiles,indent=2))
if __name__=='__main__':pack()
