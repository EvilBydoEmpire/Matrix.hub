// User-supplied John Whitney-inspired sketch, 2026-10-03.
// Formatting normalized. Author and license not supplied.
// The original copied >68 thresholds are preserved here; the native port
// derives the upper threshold from each band's 72/120/90 period.
let cycle5 = 0, cycle3 = 0, cycle4 = 0;
function setup() { createCanvas(360, 360); }
function draw() {
  background(0);
  noStroke();
  let r = 30;
  for (let i = 0; i < 360; i++) {
    let x = (180 + r * cos(radians(i-90)) + i*cycle5)%360;
    let y = 60 - r * sin(radians(i-90));
    ellipse(y, 360-x, 5, 5);
  }
  for (let i = 0; i < 360; i++) {
    let x = (180 + r * cos(radians(i-90)) + i*cycle3)%360;
    let y = 180 - r * sin(radians(i-90));
    ellipse(y, 360-x, 5, 5);
  }
  for (let i = 0; i < 360; i++) {
    let x = (180 + r * cos(radians(i-90)) + i*cycle4)%360;
    let y = 300 - r * sin(radians(i-90));
    ellipse(y, 360-x, 5, 5);
  }
  cycle5 += 1-cos(radians(cycle5*5));
  if (cycle5%72<4 || cycle5%72>68) {
    let a = map(abs(36-cycle5%72),36,32,0.001,0.1);
    cycle5 += a;
  }
  cycle3 += 1-cos(radians(cycle3*3));
  if (cycle3%120<4 || cycle3%120>68) {
    let a = map(abs(60-cycle3%120),60,56,0.001,0.1);
    cycle3 += a;
  }
  cycle4 += 1-cos(radians(cycle4*4));
  if (cycle4%90<4 || cycle4%90>68) {
    let a = map(abs(45-cycle4%90),45,41,0.001,0.1);
    cycle4 += a;
  }
}
