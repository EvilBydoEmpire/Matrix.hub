// Palette stops, and the colours of collection patterns that draw source RGB.
uint32_t compiledStop(int engine,int variant,int palette,int stop){
 if(palette==Palettes::Original){
   if(engine==PATTERNFLOW_COLLECTION)return PATTERNFLOW_METADATA[variant].originalColors[stop];
   if(engine>=0&&engine<16)return ArtDirection::Native[engine][stop];
 }
 return Palettes::Stops[Palettes::offered(palette)&&palette!=Palettes::Original?palette:0][stop];
}
void compileStops(uint32_t (&stops)[4],int engine,int variant,int palette){
 for(int c=0;c<4;c++)stops[c]=compiledStop(engine,variant,palette,c);
}
// Hue in 0..1 of an RGB colour, given its largest channel and channel spread.
float rgbHue(float r,float g,float b,float maximum,float delta){
 if(delta<=.0001f)return 0;
 float hue=maximum==r?(g-b)/delta:maximum==g?2+(b-r)/delta:4+(r-g)/delta;
 hue/=6;return hue<0?hue+1:hue;
}
// Source RGB of collection patterns, drawn with the collection's palette table.
SourceColors collectionSourceColors(int brightness){
 const ColorParams& control=colorParams[PATTERNFLOW_COLLECTION];
 return {paletteTable,control.saturation/100.f,colorScale(brightness),control.palette==Palettes::Original};
}
