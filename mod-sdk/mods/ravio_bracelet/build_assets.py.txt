from pathlib import Path
import struct
from PIL import Image,ImageDraw,ImageFont
image=Image.new('RGBA',(128,16));draw=ImageDraw.Draw(image)
font=ImageFont.truetype('C:/Windows/Fonts/segoeuib.ttf',13)
draw.text((5,-1),"Ravio's Bracelet",font=font,fill='white')
header=bytearray(64);header[4:8]=b'XETO';struct.pack_into('<Q',header,12,0xDEADBEEFDEADBEEF)
raw=image.tobytes();path=Path(__file__).parent/'assets/textures/item_name_custom/gRavioBraceletNameTex'
path.parent.mkdir(parents=True,exist_ok=True);path.write_bytes(header+struct.pack('<IIII',1,128,16,len(raw))+raw)
