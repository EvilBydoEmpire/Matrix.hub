// Patterns by stable ID; never renumber.
// Native controls are [key, label, min, max, step]. Collection controls run 0–100;
// a null label keeps that control at its default.
const defs={
 0:{name:'Starfield',params:[['w','Warp speed',0.05,1,0.01],['n','Stars',10,300,1],['s','Perspective scale',2,10,0.1]]},
 1:{name:'Warped Checkerboard',params:[['as','Speed',0.1,3,0.05],['ws','Wave scale',2,10,0.1],['d','Distortion',1,8,0.1]]},
 3:{name:'Moiré Lines',params:[['as','Speed',0.1,3,0.05],['rs1','Rotation one',0.1,3,0.05],['rs2','Rotation two',0.1,3,0.05],['lf','Line frequency',0.1,2,0.05]]},
 5:{name:'Shockwave',params:[['as','Speed',0.1,3,0.05],['wt','Wave thickness',0.1,5,0.05]]},
 6:{name:'Warp Corridors',params:[['as','Speed',0.1,3,0.05],['rs','Rotation speed',0.1,2,0.05],['ws','Warp speed',0.1,2,0.05],['bf','Band frequency',0.1,2,0.05]]},
 7:{name:'Pixel Avalanche',params:[['as','Speed',0.1,3,0.05],['sr','Spawn rate',1,10,1]]},
 9:{name:'Bayer Flow',params:[['as','Speed',0.1,3,0.05],['scale','Pattern scale',1,16,1],['contrast','Contrast',1,8,1]]},
 11:{name:'Snake',params:[['speed','Moves per second',1,20,0.25],['length','Maximum length',8,300,1],['steering','Apple instinct',0,1,0.01]]},
 12:{name:'Squares',params:[['as','Tunnel speed',0.1,4,0.05],['count','Square count',2,20,1],['thickness','Line thickness',1,4,1],['spacing','Depth spacing',0.2,2,0.05],['variance','Speed variance',0,0.5,0.01]]},
 14:{name:'Flow Sphere',params:[['speed','Flow speed',0.05,6,0.05],['scale','Field scale',0.5,4,0.05],['density','Particle density',0.1,1,0.01],['radius','Sphere radius',14,31,0.25]]}
};
const collection={
 0:['Vector Field Flow',['Turbulence','Velocity','Density',null]],
 1:['Chromatic Vortex',['Split','Swirl speed','Density',null]],
 2:['Liquid Ripple',['Refraction','Speed','Waves','Specular']],
 3:['Kaleidoscope',['Symmetry','Speed','Core depth','Twist']],
 4:['Isometric Pillars',['Grid density','Speed',null,null]],
 5:['Colour Ribbons',[null,'Speed','Ribbons','Amplitude']],
 6:['Folded Cross',['Tile size','Speed','Folds',null]],
 7:['Bitwise Shards',['Block size','Speed','Bit shift',null]],
 8:['Blooming Bioluminescence',['Cells','Speed','Plasma',null]],
 9:['Chladni Acoustic Waves',['Mode N','Mode M','Speed','Edge sharpness']],
 10:['Pulsating Solar Flare',['Density','Pulse speed','Wave height','Glow']],
 11:['Death Star',['Orbit speed','Point density','Dish size','Equator glow']],
 12:['Pendulum Waves',['Speed','Base frequency','Frequency spread',null]],
 13:['Nachtschwärmer',['Flow speed','Blob size','Gooeyness','Particles']],
 14:['Cathedral Glass',['Drift speed','Pane size','Lead sharpness',null]],
 15:['Tidal Topography',['Tide speed','Contour density','Wave depth','Line weight']],
 16:['Whitney Harmonics',['Canon speed','Voices','Orbit size',null]],
 18:['Vega Lens',['Grid','Bulge','Wave depth',null]],
 19:['Twisted Tunnel',['Ring spacing','Twist',null,'Travel speed']],
 21:['Soliton Lattice Echo',['Kink energy','Relative pace','Soliton count','Echo length']],
 22:['Ripple Cell Grid',['Wave speed','Cell size','Ripple depth','Dot width']],
 23:['Poolside',['Pool depth','Wave drift','Chop','Swell wavelength']],
 25:['Squircle Spacetime Warp',['Circle to square','Gravity lens','Grid frequency','Cell phase']],
 29:['Hypno Circles',[null,'Motion speed','Wave ratio','White intersections']],
 31:['Collatz Kelp',['Odd turn angle','Growth speed','Even turn angle','Seed count']],
 33:['Topo Vortex',['Ring count','Rotation speed','Twist',null]],
 35:['Harmonic Canon',['Canon speed','Voices','Orbit size','Trail length']],
 36:['Chladni Resonance',['Mode N','Mode M','Morph speed','Filament width']],
 38:['Satin',['Flow speed','Cell size','Warp strength','Fabric fold']],
 42:['Infinite Squares',['Travel speed','Square count','Corkscrew','Line weight']],
 43:['Motion Graphics 1968',['Motion speed','Line count','Phase spacing',null]],
 45:['Chromatic Bloom',['Bloom speed','Particle count','Petal folding','Counterpoint']],
 46:['Abyssal Vortex',['Ring count','Flow speed','Vortex twist','Braiding']],
 47:['Interference',['Motion speed','Wavelength','Distortion','Wave interference']],
 48:['Ripples',['Expansion speed','Ring width','Spawn density','Decay']],
 49:['Luminous Loom',['Weave speed','Ribbon count','Undulation','Ribbon width']],
 50:['Ferrofluid',['Flow speed','Droplets','Cohesion','Metallic reflection']],
 51:['Accretion',['Orbital speed','Disc tilt','Event horizon',null]],
 52:['Origami Flight',['Motion speed','Facet count','Folding depth','Precession']],
 53:['Signal Orchard',['Pulse speed','Trees','Branching depth','Surge']],
 54:['Resonant Orbits',['Canon speed','Particle count','Rate divergence','Echo length']],
 55:['Phase Architecture',['Motion speed','Strand count','Torsion','Precession']],
 58:['Pong Wars',['Game speed','Tile size','Ball size','Bounce variation']],
 59:['Square Orbits',['Canon speed','Particle count','Rate divergence','Echo length']],
 60:['Acid Cathedral',['Motion speed','Grid density','Arch distortion','Aliasing']],
 61:['Phase Slippage',['Motion speed','Grid density','Phase warp','Aliasing']],
 62:['Liquid Circuit',['Motion speed','Grid density','Liquid warp','Aliasing']],
 63:['Raindrops',['Drop frequency','Trail fade','Ring width','Wave speed']],
 64:['Plasma',['Flow speed','Wave scale',null,'Trail fade']],
 65:['Chrono Iris',['Pulse rate','Petal count','Exposure width','Flash depth']],
 66:['Rolling Silk',['Pulse rate','Curtain density','Exposure width','Flash depth']],
 67:['Lantern Garden',['Pulse rate','Lantern count','Exposure width','Flash depth']]
};
// The library follows the firmware's gallery order, as gallery.js lists it.
const galleryItems=galleryOrder.map((key,previewIndex)=>{
 const id=Number(key.slice(1));
 return key[0]==='n'?{key,name:defs[id].name,engine:id,variant:null,previewIndex}:{key,name:collection[id][0],engine:16,variant:id,previewIndex};
});
