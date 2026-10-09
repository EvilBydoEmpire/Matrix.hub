"""Mechanical build of the original Collatz trie, stored as compact flash tables."""
from pathlib import Path
root=Path(__file__).resolve().parent.parent
lookup={1:0};parents=[0];depth=[0];odd=[1];upto=[1,1]
for n in range(2,1601):
    v=n;path=[]
    while v not in lookup:
        path.append(v);v=3*v+1 if v%2 else v//2
    parent=lookup[v]
    for v in reversed(path):
        node=len(parents);lookup[v]=node
        parents.append(parent);depth.append(depth[parent]+1);odd.append(v%2);parent=node
    upto.append(len(parents))
def array(kind,name,values):
    return 'const '+kind+' '+name+'[] PROGMEM={\n'+''.join('  '+','.join(map(str,values[i:i+32]))+',\n' for i in range(0,len(values),32))+'};\n'
output='// Collatz trie derived from @engmung Collatz Kelp, CC-BY-SA-4.0. Generated; do not edit.\n'
for kind,name,data in [('uint16_t','COLLATZ_PARENT',parents),('uint8_t','COLLATZ_DEPTH',depth),('uint8_t','COLLATZ_ODD',odd),('uint16_t','COLLATZ_UPTO',upto)]:output+=array(kind,name,data)
(root/'src/generated/CollatzTopology.h').write_text(output)
assert len(parents)<=4096
print(len(parents),'nodes; max depth',max(depth))
