import java.nio.file.*;
import java.util.jar.*;
import jdk.internal.org.objectweb.asm.*;

// Host-only generator. Input must first be converted from the exact stock JXE.
public final class PatchMusic implements Opcodes {
    static final String CLASS="de/audi/tghu/media/hmi/mibhigh3d/MediaScreenBag4";
    static final String METHOD="dataPlayerListMain";
    static final String SIG="(Lde/audi/tghu/media/hmi/mibhigh3d/MediaScreenFactory;I)Lde/audi/atip/hmi/view/Screen;";
    static final String SCREEN="de/audi/tghu/hmi/view/ScreenWidget";
    static final String WIDGET="de/audi/tghu/hmi/view/DecoratorWidget";
    static final String ABSTRACT="Lde/audi/tghu/hmi/view/AbstractWidget;";
    static final int IMAGE=28999;
    static void call(MethodVisitor m,String n,String s){m.visitMethodInsn(INVOKEVIRTUAL,WIDGET,n,s,false);}
    static void layer(MethodVisitor m){
        m.visitInsn(DUP); m.visitTypeInsn(CHECKCAST,SCREEN);
        m.visitTypeInsn(NEW,WIDGET);m.visitInsn(DUP);
        m.visitInsn(ICONST_0);m.visitInsn(ICONST_0);m.visitIntInsn(SIPUSH,800);m.visitIntInsn(SIPUSH,480);
        m.visitMethodInsn(INVOKESPECIAL,WIDGET,"<init>","(IIII)V",false);
        m.visitInsn(DUP);m.visitLdcInsn("MusicSkinBackgroundR15");call(m,"setWidgetName","(Ljava/lang/String;)V");
        m.visitInsn(DUP);m.visitInsn(FCONST_1);m.visitInsn(FCONST_1);call(m,"setScaleFactor","(FF)V");
        m.visitInsn(DUP);m.visitInsn(ICONST_1);call(m,"setDecoratorType","(I)V");
        m.visitInsn(DUP);m.visitInsn(ICONST_1);m.visitIntInsn(NEWARRAY,T_INT);m.visitInsn(DUP);m.visitInsn(ICONST_0);m.visitIntInsn(SIPUSH,IMAGE);m.visitInsn(IASTORE);call(m,"setBitmaps","([I)V");
        m.visitInsn(DUP);m.visitIntInsn(SIPUSH,500);call(m,"setDepth","(I)V");
        m.visitMethodInsn(INVOKEVIRTUAL,SCREEN,"add","("+ABSTRACT+")V",false);
    }
    static byte[] patch(byte[] bytes){
        ClassReader r=new ClassReader(bytes);if(!CLASS.equals(r.getClassName()))throw new IllegalArgumentException("Wrong class");
        ClassWriter w=new ClassWriter(r,ClassWriter.COMPUTE_MAXS);int[] count=new int[4];
        r.accept(new ClassVisitor(ASM8,w){
            @Override public MethodVisitor visitMethod(int a,String n,String d,String s,String[] ex){
                MethodVisitor m=super.visitMethod(a,n,d,s,ex);if(!METHOD.equals(n)||!SIG.equals(d))return m;count[0]++;
                return new MethodVisitor(ASM8,m){
                    @Override public void visitIntInsn(int op,int v){if(op==SIPUSH&&v==20095)count[1]++;super.visitIntInsn(op,v);}
                    @Override public void visitMethodInsn(int op,String o,String n,String d,boolean i){if(op==INVOKESPECIAL&&o.equals(SCREEN)&&n.equals("<init>")&&d.equals("(I)V"))count[2]++;super.visitMethodInsn(op,o,n,d,i);}
                    @Override public void visitLdcInsn(Object v){if("MusicSkinBackgroundR15".equals(v))throw new IllegalArgumentException("Already patched");super.visitLdcInsn(v);}
                    @Override public void visitInsn(int op){if(op==ARETURN){count[3]++;layer(mv);}super.visitInsn(op);}
                };}
        },0);
        for(int c:count)if(c!=1)throw new IllegalArgumentException("Unexpected screen structure: "+java.util.Arrays.toString(count));return w.toByteArray();
    }
    public static void main(String[] a)throws Exception{
        if(a.length!=2)throw new IllegalArgumentException("input converted stock jar, output research-only jar");
        Path in=Paths.get(a[0]),out=Paths.get(a[1]);if(Files.exists(out))throw new IllegalArgumentException("Output exists");
        byte[] p;try(JarFile j=new JarFile(in.toFile())){JarEntry e=j.getJarEntry(CLASS+".class");if(e==null)throw new IllegalArgumentException("Missing class");try(java.io.InputStream s=j.getInputStream(e)){p=patch(s.readAllBytes());}}
        try(JarOutputStream z=new JarOutputStream(Files.newOutputStream(out,StandardOpenOption.CREATE_NEW))){z.putNextEntry(new JarEntry(CLASS+".class"));z.write(p);z.closeEntry();}
        System.out.println("RESEARCH ONLY: generated one replacement class; J9 and rendering not validated");
    }
}
