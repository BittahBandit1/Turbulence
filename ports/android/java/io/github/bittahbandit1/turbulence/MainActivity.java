package io.github.bittahbandit1.turbulence;
import android.app.*;
import android.os.*;
import android.content.*;
import android.content.pm.ActivityInfo;
import android.database.Cursor;
import android.graphics.Color;
import android.net.*;
import android.provider.*;
import android.view.*;
import android.widget.*;
import java.io.*;
import java.net.*;
import java.util.*;

public class MainActivity extends Activity implements SurfaceHolder.Callback {
 static {System.loadLibrary("turbulence");}
 private static Context application;
 private static native void nativeStart(Surface surface,String game,String user,String sys,float scale,String driverLib);
 private static native void nativeSetDriverDirectories(String root,String libraries);
 private static native boolean nativeSupportsCustomDriver();
 private static native void nativePromptStyle(int style);
 private static native void nativeStop();
 private static native void nativePause(boolean pause);
 private static native int nativeState();
 private static native String nativeMessage();
 private static native long nativeFrames();
 private static native boolean nativeMenuOpen();
 private static native void nativeInput(double[] values, int menuMask);
 private static native void nativePointer(float x,float y,boolean down);
 private static native void nativeToggleMenu();
 private static native void nativeResize();
 private final Handler handler=new Handler(Looper.getMainLooper());
 private final HashSet<Integer> keys=new HashSet<>();
 private final double[] axes=new double[18];
 private boolean playing=false,busy=false,alive=true,autoPlay=false;
 private TextView status,loading;
 private File library,user,sys;
 private SurfaceView surface;
 private TouchControls touchControls;
 private final double[] touchValues=new double[18];
 private LinearLayout gameBar;
 private final Runnable hideBar=()->{if(gameBar!=null)gameBar.setVisibility(View.GONE);};
 private void revealBar(){if(gameBar!=null){gameBar.setVisibility(View.VISIBLE);handler.removeCallbacks(hideBar);handler.postDelayed(hideBar,3000);}}
 private String pendingDisc;
 private long previousFrames=-1;
 private final Runnable tick=new Runnable(){public void run(){
  if(!alive)return;
  if(playing){
   sendInput();int state=nativeState();long frames=nativeFrames();
   if(frames>0&&loading!=null)loading.setVisibility(View.GONE);
   if(nativeMenuOpen()&&gameBar!=null)gameBar.setVisibility(View.GONE);
   if(state>=2){playing=false;keys.clear();Arrays.fill(axes,0);home(nativeMessage()+" ("+frames+" frames)");}
   else if(frames/60!=previousFrames/60){previousFrames=frames;android.util.Log.i("Turbulence","Frames="+frames+"; menu="+nativeMenuOpen());}
  }
  handler.postDelayed(this,16);
 }};
 @Override public void onCreate(Bundle saved){
  super.onCreate(saved);application=getApplicationContext();getExternalFilesDir(null);
  setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE);
  getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
  immersive();library=new File(getFilesDir(),"GXEE8P");user=new File(getFilesDir(),"user");sys=new File(getFilesDir(),"Sys");
  setUpDrivers();
  if(getIntent().getBooleanExtra("systemdriver",false))uninstallDriver();
  String driverPath=getIntent().getStringExtra("driver");
  if(driverPath!=null){try(InputStream in=new FileInputStream(driverPath)){String result=installDriver(in);android.util.Log.i("Turbulence","Driver install from "+driverPath+": "+result);}catch(Exception e){android.util.Log.e("Turbulence","Driver install failed",e);}}
  pendingDisc=getIntent().getStringExtra("disc");autoPlay=getIntent().getBooleanExtra("autoplay",false);
  home("Preparing runtime files…");busy=true;
  new Thread(()->{try{copyAssets("Sys",sys);applyDeviceDefaults();copyAsset("GFX.ini",new File(getFilesDir(),"GFX.ini"));
   runOnUiThread(()->{busy=false;home("Choose your Sonic Riders USA revision 0 ISO/CISO.");if(pendingDisc!=null){String disc=pendingDisc;pendingDisc=null;importFile(new File(disc));}else if(autoPlay&&ready())play();});
  }catch(Exception error){runOnUiThread(()->{busy=false;home(error.toString());});}},"Turbulence assets").start();
  handler.post(tick);
 }
 @Override protected void onNewIntent(Intent intent){super.onNewIntent(intent);setIntent(intent);String disc=intent.getStringExtra("disc");autoPlay=intent.getBooleanExtra("autoplay",false);if(disc!=null)importFile(new File(disc));else if(autoPlay&&ready())play();}
 private boolean ready(){return new File(library,"sys/main.dol").isFile();}
 @Override public void onWindowFocusChanged(boolean focused){super.onWindowFocusChanged(focused);if(focused)immersive();}
 private void immersive(){getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY|View.SYSTEM_UI_FLAG_FULLSCREEN|View.SYSTEM_UI_FLAG_HIDE_NAVIGATION|View.SYSTEM_UI_FLAG_LAYOUT_STABLE|View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN|View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);}
 private void home(String text){
  handler.removeCallbacks(hideBar);gameBar=null;
  LinearLayout view=new LinearLayout(this);view.setOrientation(LinearLayout.VERTICAL);view.setGravity(Gravity.CENTER);view.setPadding(32,24,32,24);view.setBackgroundColor(Color.rgb(18,23,30));
  TextView title=new TextView(this);title.setText("Turbulence");title.setTextSize(28);title.setTextColor(Color.rgb(255,207,32));view.addView(title);
  status=new TextView(this);status.setText(text);status.setTextColor(Color.WHITE);status.setGravity(Gravity.CENTER);status.setPadding(12,20,12,20);view.addView(status);
  Button choose=new Button(this);choose.setText("Choose ISO / CISO");choose.setEnabled(!busy);choose.setOnClickListener(v->{Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.setType("*/*");intent.addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(intent,10);});view.addView(choose);
  Button play=new Button(this);play.setText("Play Sonic Riders");play.setEnabled(ready()&&!busy);play.setOnClickListener(v->play());view.addView(play);
  Button logs=new Button(this);logs.setText("View gameplay log");logs.setOnClickListener(v->{try{File f=new File(user,"gameplay.log");RandomAccessFile log=new RandomAccessFile(f,"r");long size=log.length();log.seek(Math.max(0,size-60000));byte[] bytes=new byte[(int)Math.min(60000,size)];log.readFully(bytes);log.close();new AlertDialog.Builder(this).setMessage(new String(bytes,"UTF-8")).setPositiveButton("Close",null).show();}catch(Exception e){status.setText("No gameplay log yet.");}});view.addView(logs);
  TextView driverStatus=new TextView(this);driverStatus.setTextColor(Color.LTGRAY);driverStatus.setGravity(Gravity.CENTER);driverStatus.setPadding(0,16,0,4);
  boolean customOk=nativeSupportsCustomDriver();String driverName=prefs().getString("driverName","");
  driverStatus.setText(!customOk?"GPU driver: system (custom drivers need a Qualcomm Adreno GPU)":driverName.isEmpty()?"GPU driver: system":"GPU driver: "+driverName);view.addView(driverStatus);
  LinearLayout driverRow=new LinearLayout(this);driverRow.setGravity(Gravity.CENTER);
  Button installDriver=new Button(this);installDriver.setText("Install GPU driver (.zip)");installDriver.setEnabled(customOk&&!busy);installDriver.setOnClickListener(v->{Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.setType("*/*");intent.addCategory(Intent.CATEGORY_OPENABLE);startActivityForResult(intent,11);});driverRow.addView(installDriver);
  Button systemDriver=new Button(this);systemDriver.setText("Use system driver");systemDriver.setEnabled(!driverName.isEmpty()&&!busy);systemDriver.setOnClickListener(v->{uninstallDriver();home("Using the system GPU driver.");});driverRow.addView(systemDriver);
  view.addView(driverRow);
  TextView hint=new TextView(this);hint.setText("Use a controller or the on-screen controls.\nThree-finger tap opens options.");hint.setTextColor(Color.LTGRAY);hint.setGravity(Gravity.CENTER);hint.setPadding(0,12,0,0);view.addView(hint);
  setContentView(view);immersive();
 }
 private void applyDeviceDefaults() throws IOException {
  if ("motorola".equalsIgnoreCase(Build.MANUFACTURER) && "avatrn".equals(Build.DEVICE)) {
   // Sys was freshly copied above. User game settings take precedence.
   try (FileWriter defaults = new FileWriter(new File(sys, "GameSettings/GXEE8P.ini"), true)) {
    defaults.write("\n[Core]\nGPUDeterminismMode = fake-completion\n");
   }
  }
 }
 private void copyAsset(String name,File destination)throws Exception{destination.getParentFile().mkdirs();try(InputStream input=getAssets().open(name);OutputStream out=new FileOutputStream(destination)){byte[] buffer=new byte[65536];int n;while((n=input.read(buffer))>=0)out.write(buffer,0,n);}}
 private void copyAssets(String path,File destination)throws Exception {String[] children=getAssets().list(path);if(children!=null&&children.length>0){destination.mkdirs();for(String child:children)copyAssets(path+"/"+child,new File(destination,child));}else copyAsset(path,destination);}
 @Override protected void onActivityResult(int request,int result,Intent data){super.onActivityResult(request,result,data);
  if(request==11&&result==RESULT_OK&&data!=null&&data.getData()!=null){Uri uri=data.getData();busy=true;home("Installing GPU driver…");new Thread(()->{String message;try(InputStream in=getContentResolver().openInputStream(uri)){message=in==null?"The file provider could not open this driver package.":installDriver(in);}catch(Exception e){uninstallDriver();message="Driver install failed: "+e.getMessage();}final String text=message;runOnUiThread(()->{busy=false;home(text);});},"Turbulence driver install").start();return;}if(request==10&&result==RESULT_OK&&data!=null&&data.getData()!=null){Uri uri=data.getData();busy=true;home("Reading selected disc…");new Thread(()->{File source=new File(getCacheDir(),"disc-import.ciso");try(InputStream in=getContentResolver().openInputStream(uri);OutputStream out=new FileOutputStream(source)){if(in==null)throw new IOException("The file provider could not open this disc");byte[] bytes=new byte[1024*1024];int n;while((n=in.read(bytes))>=0)out.write(bytes,0,n);runOnUiThread(()->{busy=false;importFile(source);});}catch(Exception e){source.delete();runOnUiThread(()->{busy=false;home(e.toString());});}},"Turbulence disc copy").start();}}
 private void importFile(File source){
  if(busy||playing)return;
  if(ready()){if(autoPlay)play();else home("Game is already imported. Choose Play.");return;}
  busy=true;home("Importing and verifying disc…");
  new Thread(()->{try{int count=DiscImporter.extract(source,library);android.util.Log.i("Turbulence","Imported "+count+" game files");if(source.getParentFile().equals(getCacheDir()))source.delete();runOnUiThread(()->{busy=false;home("Imported and verified "+count+" files.");if(autoPlay)play();});}catch(Exception e){android.util.Log.e("Turbulence","Import failed",e);runOnUiThread(()->{busy=false;home(e.toString());});}},"Turbulence disc importer").start();
 }
 private void play(){
  if(!ready()||busy||playing)return;playing=true;previousFrames=-1;keys.clear();Arrays.fill(axes,0);
  FrameLayout root=new FrameLayout(this);surface=new SurfaceView(this);surface.getHolder().addCallback(this);root.addView(surface,new FrameLayout.LayoutParams(-1,-1));
  surface.setOnTouchListener((v,event)->{if(!nativeMenuOpen()){revealBar();if(touchControls!=null)touchControls.setVisibility(View.VISIBLE);nativePromptStyle(0);}if(event.getPointerCount()==3&&event.getActionMasked()==MotionEvent.ACTION_POINTER_DOWN){nativeToggleMenu();return true;}if(nativeMenuOpen())nativePointer(event.getX()/v.getWidth(),event.getY()/v.getHeight(),event.getActionMasked()!=MotionEvent.ACTION_UP&&event.getActionMasked()!=MotionEvent.ACTION_CANCEL);return true;});
  loading=new TextView(this);loading.setText("Starting compiled game…");loading.setTextColor(Color.WHITE);loading.setTextSize(20);loading.setGravity(Gravity.CENTER);root.addView(loading,new FrameLayout.LayoutParams(-1,-1));
  touchControls=new TouchControls(this,values->{System.arraycopy(values,0,touchValues,0,18);sendInput();},()->nativeMenuOpen(),(x,y,down)->nativePointer(x,y,down),()->nativeToggleMenu());
  root.addView(touchControls,new FrameLayout.LayoutParams(-1,-1));
  LinearLayout top=new LinearLayout(this);Button options=new Button(this);options.setText("Options");options.setOnClickListener(v->nativeToggleMenu());top.addView(options);Button exit=new Button(this);exit.setText("Exit");exit.setOnClickListener(v->new AlertDialog.Builder(this).setMessage("Are you sure?").setNegativeButton("No",null).setPositiveButton("Yes",(dialog,which)->nativeStop()).show());top.addView(exit);root.addView(top,new FrameLayout.LayoutParams(-2,-2,Gravity.TOP|Gravity.LEFT));setContentView(root);gameBar=top;revealBar();immersive();
 }
 // The game presents at 60 Hz. On 120 Hz panels (Retroid Pocket Nova) FIFO
 // presentation otherwise alternates 1 and 3 refreshes per frame; ask the
 // compositor to run this surface (and, if it can, the panel) at 60 Hz.
 private void requestSixtyHz(Surface target){
  try{
   if(Build.VERSION.SDK_INT>=31)target.setFrameRate(60f,Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE,Surface.CHANGE_FRAME_RATE_ALWAYS);
   else if(Build.VERSION.SDK_INT>=30)target.setFrameRate(60f,Surface.FRAME_RATE_COMPATIBILITY_FIXED_SOURCE);
  }catch(Exception e){android.util.Log.w("Turbulence","setFrameRate failed",e);}
  if(Build.VERSION.SDK_INT>=23){
   Display display=getWindowManager().getDefaultDisplay();Display.Mode current=display.getMode();
   for(Display.Mode mode:display.getSupportedModes()){
    if(mode.getPhysicalWidth()==current.getPhysicalWidth()&&mode.getPhysicalHeight()==current.getPhysicalHeight()&&Math.abs(mode.getRefreshRate()-60f)<1f){
     WindowManager.LayoutParams params=getWindow().getAttributes();params.preferredDisplayModeId=mode.getModeId();getWindow().setAttributes(params);
     android.util.Log.i("Turbulence","Requested display mode "+mode.getModeId()+" ("+mode.getRefreshRate()+" Hz)");break;
    }
   }
  }
 }
 public void surfaceCreated(SurfaceHolder holder){requestSixtyHz(holder.getSurface());nativeStart(holder.getSurface(),library.getAbsolutePath(),user.getAbsolutePath(),sys.getAbsolutePath(),getResources().getDisplayMetrics().density,prefs().getString("driverLib",""));}

 // Custom GPU drivers: adrenotools packages (a .zip with meta.json naming the
 // Vulkan library, e.g. Mesa Turnip builds). One driver is installed at a time.
 private File driverRoot,driverDir;
 private SharedPreferences prefs(){return getSharedPreferences("turbulence",MODE_PRIVATE);}
 private void setUpDrivers(){
  driverRoot=new File(getFilesDir(),"GpuDrivers");driverDir=new File(driverRoot,"Extracted");
  driverDir.mkdirs();new File(driverRoot,"Tmp").mkdirs();new File(driverRoot,"FileRedirect").mkdirs();
  nativeSetDriverDirectories(driverRoot.getAbsolutePath(),getApplicationInfo().nativeLibraryDir);
  String lib=prefs().getString("driverLib","");
  if(!lib.isEmpty()&&!new File(driverDir,lib).isFile())uninstallDriver();
 }
 private static void deleteTree(File file){File[] children=file.listFiles();if(children!=null)for(File child:children)deleteTree(child);file.delete();}
 private void uninstallDriver(){deleteTree(driverDir);driverDir.mkdirs();prefs().edit().remove("driverLib").remove("driverName").apply();}
 private String installDriver(InputStream input)throws IOException{
  uninstallDriver();
  String canonical=driverDir.getCanonicalPath()+File.separator;
  try(java.util.zip.ZipInputStream zip=new java.util.zip.ZipInputStream(new BufferedInputStream(input))){
   java.util.zip.ZipEntry entry;byte[] buffer=new byte[65536];
   while((entry=zip.getNextEntry())!=null){
    File target=new File(driverDir,entry.getName());
    if(!target.getCanonicalPath().startsWith(canonical)){uninstallDriver();return "Driver package rejected: unsafe file path.";}
    if(entry.isDirectory()){target.mkdirs();continue;}
    target.getParentFile().mkdirs();
    try(OutputStream out=new FileOutputStream(target)){int n;while((n=zip.read(buffer))>0)out.write(buffer,0,n);}
   }
  }
  File meta=new File(driverDir,"meta.json");
  if(!meta.isFile()){uninstallDriver();return "Not a GPU driver package: meta.json is missing.";}
  try{
   org.json.JSONObject json=new org.json.JSONObject(new String(java.nio.file.Files.readAllBytes(meta.toPath()),"UTF-8"));
   String lib=json.optString("libraryName","");
   if(lib.isEmpty()||lib.contains("/")||!new File(driverDir,lib).isFile()){uninstallDriver();return "Driver package is missing its Vulkan library ("+lib+").";}
   if(Build.VERSION.SDK_INT<json.optInt("minApi",0)){uninstallDriver();return "This driver needs Android API "+json.optInt("minApi",0)+" or newer.";}
   String name=json.optString("name",lib);String version=json.optString("driverVersion","");
   String label=version.isEmpty()?name:name+" ("+version+")";
   prefs().edit().putString("driverLib",lib).putString("driverName",label).apply();
   return "Installed GPU driver: "+label+". It is used the next time you start the game.";
  }catch(org.json.JSONException e){uninstallDriver();return "Driver package has an invalid meta.json.";}
 }
 public void surfaceChanged(SurfaceHolder holder,int format,int width,int height){nativeResize();}
 public void surfaceDestroyed(SurfaceHolder holder){if(playing)nativeStop();}
 @Override public void onBackPressed(){if(playing)nativeToggleMenu();else super.onBackPressed();}
 @Override protected void onPause(){super.onPause();if(playing){if(touchControls!=null)touchControls.release();Arrays.fill(axes,0);keys.clear();sendInput();nativePause(true);}}
 @Override protected void onResume(){super.onResume();immersive();if(playing)nativePause(false);}
 @Override protected void onDestroy(){alive=false;handler.removeCallbacks(tick);if(playing)nativeStop();super.onDestroy();}
 @Override public boolean dispatchKeyEvent(KeyEvent event){
  if(playing&&isGameKey(event.getKeyCode())){reportController(event.getDevice());if(gameBar!=null)gameBar.setVisibility(View.GONE);if(event.getAction()==KeyEvent.ACTION_DOWN)keys.add(event.getKeyCode());else if(event.getAction()==KeyEvent.ACTION_UP)keys.remove(event.getKeyCode());sendInput();return true;}return super.dispatchKeyEvent(event);
 }
 private boolean isGameKey(int key){return (key>=KeyEvent.KEYCODE_BUTTON_A&&key<=KeyEvent.KEYCODE_BUTTON_MODE)||(key>=KeyEvent.KEYCODE_DPAD_UP&&key<=KeyEvent.KEYCODE_DPAD_CENTER);}
 private double axis(MotionEvent event,int axis){float value=event.getAxisValue(axis);return Math.abs(value)<0.15?0:value;}
 @Override public boolean onGenericMotionEvent(MotionEvent event){
  if(playing&&(event.getSource()&InputDevice.SOURCE_JOYSTICK)==InputDevice.SOURCE_JOYSTICK){reportController(event.getDevice());axes[14]=axis(event,MotionEvent.AXIS_X);axes[15]=-axis(event,MotionEvent.AXIS_Y);axes[16]=axis(event,MotionEvent.AXIS_Z);axes[17]=-axis(event,MotionEvent.AXIS_RZ);axes[12]=Math.max(0,Math.max(axis(event,MotionEvent.AXIS_LTRIGGER),axis(event,MotionEvent.AXIS_BRAKE)));axes[13]=Math.max(0,Math.max(axis(event,MotionEvent.AXIS_RTRIGGER),axis(event,MotionEvent.AXIS_GAS)));sendInput();return true;}return super.onGenericMotionEvent(event);
 }
 private void reportController(InputDevice device){
  if(device==null)return;
  if(touchControls!=null&&touchControls.getVisibility()==View.VISIBLE){touchControls.release();touchControls.setVisibility(View.INVISIBLE);}
  String name=device.getName().toLowerCase(Locale.ROOT);
  nativePromptStyle(device.getVendorId()==0x054c||name.contains("playstation")||name.contains("dualshock")||name.contains("dualsense")||name.contains("wireless controller")?2:1);
 }
 private boolean held(int key){return keys.contains(key);}
 private static final int[] BUTTON_CODES = {96,97,99,100,102,108,19,20,21,22};
 private final double[] values = new double[18];
 private void sendInput(){
  System.arraycopy(axes,0,values,0,values.length);for(int i=0;i<18;i++)if(touchValues[i]!=0)values[i]=touchValues[i];
  for(int i=0;i<BUTTON_CODES.length;i++)if(held(BUTTON_CODES[i]))values[i]=1;
  if(held(KeyEvent.KEYCODE_DPAD_CENTER))values[0]=1;
  if(values[14]==0)values[14]=values[9]-values[8];
  if(values[15]==0)values[15]=values[6]-values[7];
  if(held(104)){values[10]=1;values[12]=1;}if(held(105)){values[11]=1;values[13]=1;}
  if(values[12]>.9)values[10]=1;if(values[13]>.9)values[11]=1;
  int mask=0;if(held(103))mask|=1;if(values[6]>0||values[15]>.5)mask|=2;if(values[7]>0||values[15]<-.5)mask|=4;if(values[8]>0||values[14]<-.5)mask|=8;if(values[9]>0||values[14]>.5)mask|=16;if(values[1]>0)mask|=32;if(values[0]>0)mask|=64;
  nativeInput(values, mask);
 }
 // Native filesystem helpers. The importer opens SAF documents through ContentResolver.
 public static int openContent(String uri,String mode){try(ParcelFileDescriptor fd=application.getContentResolver().openFileDescriptor(Uri.parse(uri),mode)){return fd==null?-1:fd.detachFd();}catch(Exception e){return -1;}}
 public static boolean deleteContent(String uri){try{return DocumentsContract.deleteDocument(application.getContentResolver(),Uri.parse(uri));}catch(Exception e){return false;}}
 public static long contentSize(String uri){try(Cursor c=application.getContentResolver().query(Uri.parse(uri),new String[]{DocumentsContract.Document.COLUMN_MIME_TYPE,OpenableColumns.SIZE},null,null,null)){if(c!=null&&c.moveToFirst())return DocumentsContract.Document.MIME_TYPE_DIR.equals(c.getString(0))?-2:c.getLong(1);}catch(Exception ignored){}return -1;}
 public static String contentName(String uri){try(Cursor c=application.getContentResolver().query(Uri.parse(uri),new String[]{OpenableColumns.DISPLAY_NAME},null,null,null)){if(c!=null&&c.moveToFirst())return c.getString(0);}catch(Exception ignored){}return "";}
 public static String[] childNames(String directory,boolean uriNames){ArrayList<String> names=new ArrayList<>();try{Uri tree=Uri.parse(directory);String id=DocumentsContract.isDocumentUri(application,tree)?DocumentsContract.getDocumentId(tree):DocumentsContract.getTreeDocumentId(tree);Uri children=DocumentsContract.buildChildDocumentsUriUsingTree(tree,id);try(Cursor c=application.getContentResolver().query(children,new String[]{DocumentsContract.Document.COLUMN_DOCUMENT_ID,DocumentsContract.Document.COLUMN_DISPLAY_NAME},null,null,null)){while(c!=null&&c.moveToNext())names.add(uriNames?DocumentsContract.buildDocumentUriUsingTree(tree,c.getString(0)).toString():c.getString(1));}}catch(Exception ignored){}return names.toArray(new String[0]);}
 public static String[] searchContent(String directory,String[] extensions,boolean recursive){ArrayList<String> result=new ArrayList<>();ArrayDeque<String> queue=new ArrayDeque<>();queue.add(directory);int scanned=0;while(!queue.isEmpty()&&scanned++<10000){for(String uri:childNames(queue.remove(),true)){if(contentSize(uri)==-2){if(recursive)queue.add(uri);}else{String name=contentName(uri).toLowerCase(Locale.ROOT);boolean matches=extensions.length==0;for(String ext:extensions)if(name.endsWith(ext.toLowerCase(Locale.ROOT)))matches=true;if(matches)result.add(uri);}}}return result.toArray(new String[0]);}
 private static LinkProperties network(){ConnectivityManager m=(ConnectivityManager)application.getSystemService(CONNECTIVITY_SERVICE);return m==null?null:m.getLinkProperties(m.getActiveNetwork());}
 private static int address(InetAddress a){byte[] b=a.getAddress();if(b.length!=4)return 0;return (b[0]&255)|((b[1]&255)<<8)|((b[2]&255)<<16)|((b[3]&255)<<24);}
 public static int networkAddress(){try{LinkProperties p=network();if(p!=null)for(LinkAddress a:p.getLinkAddresses())if(a.getAddress() instanceof Inet4Address)return address(a.getAddress());}catch(Exception ignored){}return 0;}
 public static int networkPrefix(){try{LinkProperties p=network();if(p!=null)for(LinkAddress a:p.getLinkAddresses())if(a.getAddress() instanceof Inet4Address)return a.getPrefixLength();}catch(Exception ignored){}return 0;}
 public static int networkGateway(){try{LinkProperties p=network();if(p!=null)for(RouteInfo route:p.getRoutes())if(route.isDefaultRoute()&&route.getGateway() instanceof Inet4Address)return address(route.getGateway());}catch(Exception ignored){}return 0;}
}
