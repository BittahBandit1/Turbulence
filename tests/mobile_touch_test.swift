import Foundation
import CoreGraphics
@main struct TouchTests {
 static func main() {
  for size in [CGSize(width:844,height:390),CGSize(width:1024,height:768),CGSize(width:390,height:844)] {
   let bounds=CGRect(origin:.zero,size:size)
   let safe=bounds.insetBy(dx:20,dy:20)
   let layout=MobileControlLayout(bounds:bounds,safe:safe)
   for region in layout.regions {
    precondition(safe.contains(region.frame), "Control outside safe area")
    precondition(layout.control(at:CGPoint(x:region.frame.midX,y:region.frame.midY))==region.control)
   }
  }
  let held:Set<MobileControl>=[.left,.jump,.boost,.driftLeft,.driftRight]
  let values=MobileControlLayout.gameInput(held:held)
  precondition(values[14] == -1 && values[0] == 1 && values[1] == 1)
  precondition(values[10] == 1 && values[11] == 1 && values[12] == 1 && values[13] == 1)
  precondition(MobileControlLayout.gameInput(held:[])[0] == nil)
  print("PASS: Sonic Riders touch layout, jump, boost, independent drift triggers and release")
 }
}
