// John Whitney-inspired sketch, supplied privately on 2026-10-03 without an
// author or licence. Formatting normalised.
let bg = "#171815";
let palette = ["#FBFAF8", "#F3EA83", "#C85D4D", "#92302F", "#568C44", "#4A64C7", "#CF6DA0"];
let sqrs = [];
let sqSize = 10;
let numSq = 200;
function setup() {
  createCanvas(600, 600);
  angleMode(DEGREES);
  for (let i = 0; i < numSq; i++) {
    let angle = i * (360 / numSq);
    let phase = random(100);
    let sqPalette = i % palette.length;
    sqrs.push({ angle, phase, color: sqPalette });
  }
}
function draw() {
  background(bg);
  translate(width / 2, height / 2);
  for (let sq of sqrs) {
    let speed = 0.5;
    let amp = 100;
    let waveOffset = sin(frameCount * speed + sq.phase) * amp;
    let baseAngle = 0;
    let spacing = 1;
    angle = baseAngle + sq.angle * spacing;
    let r = 20;
    let x = cos(sq.angle) * (r + waveOffset);
    let y = sin(sq.angle) * (r + waveOffset);
    noStroke();
    fill(palette[sq.color]);
    rectMode(CENTER);
    rect(x, y, sqSize, sqSize);
  }
}
