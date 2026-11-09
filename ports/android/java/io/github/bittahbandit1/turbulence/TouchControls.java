package io.github.bittahbandit1.turbulence;

import android.content.Context;
import android.graphics.*;
import android.view.*;
import java.util.*;
import java.util.function.BooleanSupplier;

// Touch mapping is Sonic Riders specific; the lifecycle/import host derives from ReFare.
public final class TouchControls extends View {
 public interface Input { void update(double[] values); }
 public interface MenuPointer { void update(float x,float y,boolean down); }
 private final Input input;
 private final BooleanSupplier menuOpen;
 private final MenuPointer menuPointer;
 private final Runnable toggleMenu;
 private final Map<Integer,PointF> fingers=new HashMap<>();
 private final Paint paint=new Paint(Paint.ANTI_ALIAS_FLAG);
 private RectF pad;
 private int stickId=-1;
 private RectF[] buttons;
 private final String[] labels={"A","B","L","R","START"};
 public TouchControls(Context context,Input input,BooleanSupplier menu,MenuPointer pointer,Runnable toggle) {
  super(context);this.input=input;menuOpen=menu;menuPointer=pointer;toggleMenu=toggle;setFocusable(false);
 }
 private void layoutControls() {
  float d=getResources().getDisplayMetrics().density;
  float unit=Math.min(getWidth()/720f,getHeight()/360f);
  float edge=Math.max(12*d,16*unit),size=64*unit,gap=10*unit;
  float bottom=getHeight()-edge,right=getWidth()-edge;
  pad=new RectF(edge,bottom-160*unit,edge+160*unit,bottom);
  buttons=new RectF[]{new RectF(right-size,bottom-size,right,bottom),
   new RectF(right-size*2-gap,bottom-size,right-size-gap,bottom),
   new RectF(right-size*2-gap,bottom-size*2-gap,right-size-gap,bottom-size-gap),
   new RectF(right-size,bottom-size*2-gap,right,bottom-size-gap),
   new RectF(getWidth()/2f-44*unit,edge,getWidth()/2f+44*unit,edge+36*unit)};
 }
 @Override protected void onDraw(Canvas canvas) {
  super.onDraw(canvas);layoutControls();if(menuOpen.getAsBoolean())return;
  paint.setColor(Color.argb(70,0,0,0));canvas.drawOval(pad,paint);
  paint.setColor(Color.argb(150,255,255,255));paint.setTextAlign(Paint.Align.CENTER);paint.setTextSize(pad.height()*.2f);
  canvas.drawText("＋",pad.centerX(),pad.centerY()+paint.getTextSize()*.35f,paint);
  for(int i=0;i<buttons.length;i++) {
   paint.setColor(Color.argb(80,0,0,0));canvas.drawRoundRect(buttons[i],16,16,paint);
   paint.setColor(Color.argb(190,255,255,255));paint.setTextSize(buttons[i].height()*.35f);
   canvas.drawText(labels[i],buttons[i].centerX(),buttons[i].centerY()+paint.getTextSize()*.35f,paint);
  }
 }
 public void release(){fingers.clear();stickId=-1;input.update(new double[18]);invalidate();}
 @Override public boolean onTouchEvent(MotionEvent event) {
  layoutControls();int action=event.getActionMasked();
  if(event.getPointerCount()==3&&action==MotionEvent.ACTION_POINTER_DOWN){release();toggleMenu.run();return true;}
  if(menuOpen.getAsBoolean()) {
   release();menuPointer.update(event.getX()/Math.max(1,getWidth()),event.getY()/Math.max(1,getHeight()),action!=MotionEvent.ACTION_UP&&action!=MotionEvent.ACTION_CANCEL);return true;
  }
  if(action==MotionEvent.ACTION_CANCEL){release();return true;}
  if((action==MotionEvent.ACTION_DOWN||action==MotionEvent.ACTION_POINTER_DOWN)&&pad.contains(event.getX(event.getActionIndex()),event.getY(event.getActionIndex())))stickId=event.getPointerId(event.getActionIndex());
  for(int i=0;i<event.getPointerCount();i++) {
   int id=event.getPointerId(i);
   if((action==MotionEvent.ACTION_UP||action==MotionEvent.ACTION_POINTER_UP)&&i==event.getActionIndex()){fingers.remove(id);if(id==stickId)stickId=-1;}
   else fingers.put(id,new PointF(event.getX(i),event.getY(i)));
  }
  double[] values=new double[18];
  for(Map.Entry<Integer,PointF> entry:fingers.entrySet()) {
   PointF p=entry.getValue();
   if(entry.getKey()==stickId) {values[14]=Math.max(-1,Math.min(1,(p.x-pad.centerX())/(pad.width()*.35)));values[15]=Math.max(-1,Math.min(1,(pad.centerY()-p.y)/(pad.height()*.35)));}
   for(int i=0;i<buttons.length;i++)if(buttons[i].contains(p.x,p.y)) {
    if(i<2)values[i]=1;else if(i==2){values[10]=1;values[12]=1;}else if(i==3){values[11]=1;values[13]=1;}else values[5]=1;
   }
  }
  input.update(values);invalidate();return true;
 }
}
