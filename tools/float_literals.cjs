// Mechanical float-only normalization for Xtensa shader kernels. Comments,
// strings, identifiers, hex values and existing suffixes are preserved.
const fs=require('fs');
for(const path of process.argv.slice(2)){
  let changed=0;
  const source=fs.readFileSync(path,'utf8');
  const tokens=/\/\/[^\n]*|\/\*[\s\S]*?\*\/|"(?:\\.|[^"\\])*"|'(?:\\.|[^'\\])*'|\b0[xX][\da-fA-F]+[uUlL]*|\b[A-Za-z_]\w*|(?:\d+\.\d*|\.\d+)(?:[eE][+-]?\d+)?[fFlL]?|\d+[eE][+-]?\d+[fFlL]?/g;
  const output=source.replace(tokens,token=>{
    if(/^(?:\d+\.\d*|\.\d+|\d+[eE])/.test(token)&&!/[fFlL]$/.test(token)){changed++;return token+'f';}
    return token;
  });
  fs.writeFileSync(path,output);console.log(path+': '+changed+' float suffixes');
}
