"""One-time removal of retired gallery variants 56/57 and their preview pixels."""
import json
import re
from pathlib import Path
from PIL import Image

root=Path(__file__).resolve().parent.parent
catalog=json.loads((root/'assets/pattern-catalog.json').read_text())
removed=[i for i,p in enumerate(catalog) if p.get('engine')==16 and p.get('variant') in (56,57)]
if len(removed)!=2:raise SystemExit('Removal already applied or unexpected catalog; refusing to change assets')
atlas=Image.open(root/'assets/pattern-preview-atlas.png').convert('RGB')
assert atlas.height==len(catalog)*28
kept=[i for i in range(len(catalog)) if i not in removed]
result=Image.new('RGB',(atlas.width,len(kept)*28))
for row,old in enumerate(kept):result.paste(atlas.crop((0,old*28,atlas.width,(old+1)*28)),(0,row*28))
result.save(root/'assets/pattern-preview-atlas.png',optimize=True)
(root/'assets/pattern-catalog.json').write_text(json.dumps([catalog[i] for i in kept],indent=2)+'\n')
path=root/'64x64_matrix_01.ino';source=path.read_text()
meta=json.loads(re.search(r'const pfMeta=(.*);',source)[1])
authors=json.loads(re.search(r'const pfAuthors=(.*);',source)[1])
ids=json.loads(re.search(r'const collectionPatternIds=(.*);',source)[1])
for v in (56,57):meta[v]=['Removed',['Unused']*4];authors[v]=''
for name,value in [('pfMeta',meta),('pfAuthors',authors),('collectionPatternIds',[i for i in ids if i not in (56,57)])]:
 source=re.sub(r'const '+name+r'=.*;',lambda _: 'const '+name+'='+json.dumps(value,separators=(',',':'),ensure_ascii=False)+';',source)
for a,b in [('>61 patterns<','>59 patterns<'),('state.catalog?.length||61','state.catalog?.length||59'),
 ('Photo: RAW left / FIX right','Chart: RAW left / FIX right'),('Photo: simulated 3-bit / native 4-bit','Chart: simulated 3-bit / native 4-bit'),
 ('Comparison photo','Comparison chart'),('IMG_4786','Grayscale'),('IMG_4704','RGB ramps'),
 ('Photo gamma /','Chart gamma /'),('>Photo correction<','>Chart correction<'),
 ('Gamma affects photos only;','Gamma affects comparison charts only;'),('Neutral RGB / original photo output','Neutral RGB / uncorrected chart')]:source=source.replace(a,b)
path.write_text(source)
print('Removed preview rows',removed,'; gallery now',len(kept))
