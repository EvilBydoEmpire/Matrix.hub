// User-supplied sketch, formatting normalized from chat (2026-10-03).
// Recreating 'Experiments in Motion Graphics', 1968, by John Whitney.
// Original artwork: https://www.youtube.com/watch?v=jIv-EcX9tUs&t=264s
// Recreated by Kenny Kim. No license statement was supplied with this sketch.
var totalLines;
var t;
function setup() {
  initializeFields();
  background(color(0x07, 0x18, 0x1d));
  createCanvas(1080, 810);
}
function draw() {
  background(color(0x07, 0x18, 0x1d));
  translate(width / 2, height / 2);
  strokeWeight(2.5);
  for (var i = 0; i < totalLines; i++) {
    stroke(color(0xdc, 0xd5, 0xbf));
    line(x1(t + i), y1(t + i), x2(t + i), y2(t + i));
  }
  for (var j = 0; j < totalLines; j++) {
    stroke(color(0xa3, 0x4a, 0x80));
    line(x3(t + j), y3(t + j), x4(t + j), y4(t + j));
  }
  t += 1.2;
  if (frameCount % 180 == 0) delay(800);
}
function x1(t) { return sin(t / 5) * 100; }
function y1(t) { return sin(t / 10) * 150; }
function x2(t) { return sin(t / 20) * 200; }
function y2(t) { return sin(t / 40) * 350; }
function x3(t) { return cos(t / 5) * 100; }
function y3(t) { return sin(t / 10) * 150; }
function x4(t) { return cos(t / 20) * 200; }
function y4(t) { return sin(t / 40) * 350; }
function initializeFields() { totalLines = 31; t = 0; }
