"""Mechanical wiring of the curated catalog into the legacy monolithic sketch."""
from pathlib import Path
import re
raise SystemExit('Completed one-time wiring. Current firmware is maintained directly; do not rerun this historical migration.')
root=Path(__file__).resolve().parent.parent
path=root/'64x64_matrix_01.ino';s=path.read_text()
s=s.replace('uint32_t paletteStops[NUM_ANIMATIONS][7][4]','uint32_t paletteStops[NUM_ANIMATIONS][11][4]').replace('uint32_t paletteStopPatches[NUM_ANIMATIONS][NUM_PATCH_SLOTS][7][4]','uint32_t paletteStopPatches[NUM_ANIMATIONS][NUM_PATCH_SLOTS][11][4]')
s=s.replace('void parseUsbSerialLine(', '#include "src/patterns/LibrarySettings.inc"\n\nvoid parseUsbSerialLine(',1)
s=s.replace('  loadSettings();\n  applyCurrentBrightness();','  loadSettings();\n  configureCuratedLibrary();\n  applyCurrentBrightness();',1)
# Exact old table references only: do not widen any unrelated parameter limits.
s=s.replace('constrain((int)controls.palette, 0, 6)','constrain((int)controls.palette, 0, 10)').replace('constrain((int)color.palette, 0, 6)','constrain((int)color.palette, 0, 10)').replace('constrain((int)colorParams[animIndex].palette, 0, 6)','constrain((int)colorParams[animIndex].palette, 0, 10)').replace('colorParams[animIndex].palette = clampInt(i, 0, 6)','colorParams[animIndex].palette = clampInt(i, 0, 10)')
s=s.replace("'Neon','Custom'];","'Neon','Custom','Original','Ember','Ice','Reef'];")
a=s.index('  static const uint32_t defaultPaletteStops[7][4]');b=s.index('\n  for (int animation',a)
s=s[:a]+'  const auto& defaultPaletteStops=LIBRARY_PALETTES;'+s[b:]
# Original bank is per native engine, not a global rainbow.
s=s.replace('    for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {\n      stylePatches', '    for(int c=0;c<4;c++)paletteStops[animation][7][c]=compiledStop(animation,0,7,c);\n    for (int patch = 0; patch < NUM_PATCH_SLOTS; patch++) {\n      stylePatches',1)
s=s.replace('      memcpy(paletteStopPatches[animation][patch], defaultPaletteStops,\n             sizeof(defaultPaletteStops));','      memcpy(paletteStopPatches[animation][patch],paletteStops[animation],sizeof(paletteStops[animation]));',1)
s=re.sub(r'preferences\.putBytes\("palettes_p", paletteStopPatches,\s*sizeof\(paletteStopPatches\)\);','writePalettePatches(preferences);',s)
s=re.sub(r'preferences\.getBytes\("palettes_p", paletteStopPatches,\s*sizeof\(paletteStopPatches\)\)','readPalettePatches(preferences)',s)
# Old pre-v15 prefix reads use the same migration decoder; lengths are handled there.
s=re.sub(r'preferences\.getBytes\("palettes_p", paletteStopPatches,\s*sizeof\(uint32_t\) \* OLD_ANIMATION_COUNT \* NUM_PATCH_SLOTS \* 7 \* 4\);','readPalettePatches(preferences);',s)
s=s.replace('  const uint8_t desiredSize=variant == 25 ? 32 :\n        (patternflowQuality[variant] == 0 ? 64 : patternflowRenderSize[variant]);','  const uint8_t desiredSize=patternflowQuality[variant] == 0 ? 64 : patternflowRenderSize[variant];')
s=s.replace('modPfc.control,huePalette,','modPfc.control,huePalette,importedRgbPalette,').replace('modPfc.control, huePalette,','modPfc.control, huePalette, importedRgbPalette,')
s=s.replace('      ensureHuePalette(255, 255, modPfc.b);','      ensureHuePalette(255, 255, modPfc.b);\n      ensureImportedRgbPalette(modPfc.b);')
s=s.replace('void loadPatch(Animation anim, int patchIndex) {\n  if (patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS) return;', 'void loadPatch(Animation anim, int patchIndex) {\n  if (patchIndex < 0 || patchIndex >= NUM_PATCH_SLOTS || !isAvailableEngine(anim)) return;\n  if(anim==PATTERNFLOW_COLLECTION){loadVariantPatch(pfcParams.variant,patchIndex);return;}\n  patternflowPack.reset(255);')
a=s.index('void advanceAutoCyclePattern()');b=s.index('\nvoid updatePlaybackFades()',a)
s=s[:a]+'''void advanceAutoCyclePattern() {
  luaHelloEnabled=false;
  int next=(galleryIndex(currentAnimation,pfcParams.variant)+1)%GALLERY_PATTERN_COUNT;
  if(next<CORE_PATTERN_COUNT){currentAnimation=(Animation)CORE_PATTERN_IDS[next];loadPatch(currentAnimation,defaultPatches[currentAnimation]);}
  else{currentAnimation=PATTERNFLOW_COLLECTION;int v=next-CORE_PATTERN_COUNT;loadVariantPatch(v,variantDefaults[v]);}
}
'''+s[b:]
s=s.replace('if (value < 0 || value >= NUM_ANIMATIONS) success = false;', 'if (!isAvailableEngine(value)||value==PATTERNFLOW_COLLECTION) success = false;',1)
a=s.index('      loadPatch(currentAnimation, defaultPatches[currentAnimation]);',s.index('} else if (!strcmp(commandName, "setPatternVariant"))'))
b=s.index('      autoCycleTransitionPhase',a)
s=s[:a]+'      loadVariantPatch(variant,variantDefaults[variant]);\n      const PatternflowMetadata& metadata=PATTERNFLOW_METADATA[variant];\n'+s[b:]
s=s.replace('      savePatchToPreferences(animIndex, patchIndex);\n      message = String("Saved patch ")', '      if(animIndex==PATTERNFLOW_COLLECTION)success=saveVariantPatch(patchIndex);else savePatchToPreferences(animIndex, patchIndex);\n      message = success?String("Saved patch ")')
s=s.replace('(patchIndex + 1);\n    }\n  } else if (!strcmp(commandName, "loadPatch"))', '(patchIndex + 1):String("Could not save patch to device storage");\n    }\n  } else if (!strcmp(commandName, "loadPatch"))')
s=s.replace('    savePatchToPreferences(animIndex, patchIndex);\n    message = String(animationNames[animIndex]) +', '    if(animIndex==PATTERNFLOW_COLLECTION)success=saveVariantPatch(patchIndex);else savePatchToPreferences(animIndex, patchIndex);\n    message = success?String(animationNames[animIndex]) +')
s=s.replace('" parameters saved permanently to default patch " + (patchIndex + 1);', '" parameters saved permanently to default patch " + (patchIndex + 1):String("Could not save default to device storage");')
s=s.replace('      defaultPatches[animIndex] = patchIndex;','      defaultPatches[animIndex] = patchIndex;\n      if(animIndex==PATTERNFLOW_COLLECTION){const uint8_t previous=variantDefaults[pfcParams.variant];variantDefaults[pfcParams.variant]=patchIndex;success=saveVariantPatch(patchIndex);if(!success){variantDefaults[pfcParams.variant]=previous;defaultPatches[animIndex]=previous;}}')
s=s.replace('animIndex < 0 || animIndex >= NUM_ANIMATIONS || patchIndex', '!isAvailableEngine(animIndex) || patchIndex')
s=s.replace('if (animIndex < 0 || animIndex >= NUM_ANIMATIONS || name == nullptr)', 'if (!isAvailableEngine(animIndex) || name == nullptr)')
# Remove raymarcher-specific presentation now that the torus was discarded.
s=s.replace("state.currentPattern===16&&Number(state.params.variant)!==25",'state.currentPattern===16')
s=s.replace('(pfcParams.variant==25 ? 32 :\n      (patternflowQuality[pfcParams.variant]==0 ? 64 : patternflowRenderSize[pfcParams.variant]))','(patternflowQuality[pfcParams.variant]==0 ? 64 : patternflowRenderSize[pfcParams.variant])')
s=s.replace('  JsonArray bookmarks = doc.createNestedArray("bookmarks");','  doc["previewFrames"]=PATTERN_PREVIEW_FRAMES;\n  doc["previewIntervalMs"]=PATTERN_PREVIEW_INTERVAL_MS;\n  JsonArray catalog=doc.createNestedArray("catalog");\n  for(int i=0;i<CORE_PATTERN_COUNT;i++){JsonObject item=catalog.createNestedObject();int id=CORE_PATTERN_IDS[i];item["name"]=animationNames[id];item["engine"]=id;item["variant"]=nullptr;}\n  for(int v=0;v<PATTERNFLOW_PATTERN_COUNT;v++){JsonObject item=catalog.createNestedObject();item["name"]=PATTERNFLOW_METADATA[v].name;item["engine"]=16;item["variant"]=v;}\n  JsonArray bookmarks = doc.createNestedArray("bookmarks");')
# Native discarded render cases are removed from the frame switch only.
a=s.index('void drawAnimation(') if 'void drawAnimation(' in s else s.index('    case STARFIELD:',s.index('void loop()'))
# These sections are brace-delimited and end just before the following case.
for name in ['MOIRE_RINGS','WARPING_CHECKERS','KALEIDOSCOPE','PERSPECTIVE_FLOW','FLOW_FIELD_2D']:
    match=re.search(r'^    case '+name+r': \{',s[a:],re.M)
    if match:
        start=a+match.start();next_case=re.search(r'^    case \w+: \{',s[a+match.end():],re.M)
        assert next_case
        end=a+match.end()+next_case.start();s=s[:start]+s[end:]
path.write_text(s)
print('Curated IDs, sparse gradients, independent variant patches and frame renderer wired.')
