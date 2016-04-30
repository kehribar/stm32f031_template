// Enclosure for
// http://blog.kehribar.me/build/2015/12/06/polyphonic-fm-synthesizer-with-stm32f031.html

$fn=100;

case(drawPart=1 /*0=box, 1=lid, 2==both*/); 

/**
 * Constructs the enclosure
 *
 * @param drawPart 0=box, 1=lid, 2==both
 */
module case (drawPart) {

    wall=1.5;                  // wall thickness

    boardWidth=50;             // board width
    boardLength=50;            // board length
    boardHeight=1.4;           // board height
    holePadding=2.5;           // center of the drill hole relative to the board edges
    holeRadius=1.4;            // drill hole radius

    innerMargin=0.5;           // margin, added to board dimensions, makes the inner dimensions larger * 2 in x/y
    innerHeight=29;            // inner height of the box, no margins or compensation for standoff heights / board height is added

    standoffR=3;               // standoff radius
    standoffH=2;               // standoff height

    usbSocketEnabled = 1;      // if you plan on using a usb socket on a breakout board to provide power 
    usbSocketHoleWidth = 11;   // width of the usb socket hole
    usbSocketHoleHeight = 6;   // height of the usb socket hole
    usbSocketIndentWidth = 13; // indentation width of the usb socket in the lid
    usbSocketOffset = 35;      // x-offset of the usb socket hole

    module box() {
        difference(){

            // walls
            translate([-(wall+innerMargin),-(wall+innerMargin),-wall]) {
                cube ([
                    boardWidth+(2*wall)+(innerMargin*2),
                    boardLength+(2*wall)+(innerMargin*2),
                    innerHeight+1*wall]);
            }
            translate([-(innerMargin),-(innerMargin),0]) {
                cube ([boardWidth+(innerMargin*2),boardLength+(innerMargin*2),innerHeight+1]);
            }

            /**
             * Holes for MIDI, Power and Jacks are hardcoded, it will depend on the socket types you use
             * and the general design.
             */

            // midi socket
            translate([6.5+innerMargin,-(innerMargin+wall+1),boardHeight+standoffH+1])
                cube([19,25,19]);

            
             // jacks, hardcoded for now
            translate([boardLength-1,boardWidth/1.5,innerHeight-innerHeight/3]) {
                rotate([90,0,90]) {
                    translate([-8,0,0])cylinder(h=10,r=4.5);
                    translate([8,0,0]) cylinder(h=10,r=4.5);
                }
            }
                
            if (usbSocketEnabled) {
                // usb socket
                translate([usbSocketOffset+wall+innerMargin,-(innerMargin+wall+1),innerHeight-5])
                    cube([usbSocketHoleWidth,10,usbSocketHoleHeight]);
            }
        }

        // standoffs
        translate([holePadding+innerMargin, holePadding+innerMargin,0])
            standoff(standoffH, standoffR, holeRadius);
        translate([boardWidth-holePadding-innerMargin, holePadding+innerMargin,0])
            standoff(standoffH, standoffR, holeRadius);
        translate([holePadding+innerMargin, boardLength-holePadding-innerMargin,0])
            standoff(standoffH, standoffR, holeRadius);
        translate([boardWidth-holePadding-innerMargin, boardLength-holePadding-innerMargin,0])
            standoff(standoffH, standoffR, holeRadius);
    }

    module lid() {
        // lid
        translate([-(wall+innerMargin),-(wall+innerMargin),-wall]) {
            cube ([boardWidth+(2*wall)+(innerMargin*2),boardLength+(2*wall)+(innerMargin*2),wall]);
        }

        // lip
        translate([-innerMargin,-innerMargin,0]) {
            difference(){
                cube ([boardWidth+(innerMargin*2),boardLength+(innerMargin*2),wall]);

                if (usbSocketEnabled) {
                    // my prints work / align better like this. odd. some wonky slicer aliasing?
                    oddOffset = -0.5;
                    
                    // indentation for the usb socket
                    translate([
                        boardWidth
                            -usbSocketOffset
                            -usbSocketHoleWidth
                            -wall
                            -(usbSocketIndentWidth-usbSocketHoleWidth) / 2
                            +oddOffset,-1,0])
                    cube([usbSocketIndentWidth,17,wall*2]);                    
                }
            }
        }
    }

    if (drawPart == 0){
        box();
    }
    if (drawPart == 1) {
        lid();
    }
    if (drawPart == 2) {
        box();
        rotate([0,180,0]) translate([-(boardWidth ),0,-(innerHeight+5)]) lid();
    }
}


module standoff(standoffH, standoffR, holeRadius) {
    difference(){
        cylinder(h=standoffH,r=standoffR);
        cylinder(h=standoffH,r=holeRadius);
    }
}
