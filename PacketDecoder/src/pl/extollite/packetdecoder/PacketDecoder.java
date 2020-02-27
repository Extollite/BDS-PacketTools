package pl.extollite.packetdecoder;

import com.nukkitx.protocol.bedrock.v389.Bedrock_v389;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import java.io.*;

public class PacketDecoder {

    public static void main(String[] args){
        final File logFolder = new File("log");
        if(!logFolder.exists()){
            System.out.println("Log folder don't exists!");
            return;
        }
        File[] dateFolders = logFolder.listFiles();
        if(dateFolders == null){
            System.out.println("Log folder is empty!");
            return;
        }
        for(File dateFolder : dateFolders){
            File[] playerFolders = dateFolder.listFiles();
            if(playerFolders == null){
                System.out.println("Date folder is empty!");
                return;
            }
            for(File playerFolder : playerFolders){
                File[] packetLogs = playerFolder.listFiles();
                if(packetLogs == null){
                    System.out.println("Player folder is empty!");
                    return;
                }
                for(File packetLog : packetLogs){
                    try (
                            InputStream reader = new FileInputStream(packetLog);
                    ) {
                        File file = new File(playerFolder.getAbsolutePath().replace("log"+File.separator,"log-output"+File.separator));
                        if(!file.exists()){
                            file.mkdirs();
                        }
                        file = new File(packetLog.getAbsolutePath().replace("log"+File.separator,"log-output"+File.separator));
                        if(!file.exists()){
                            file.createNewFile();
                        }
                        BufferedWriter writer = new BufferedWriter(new FileWriter(file));
                        int readByte;

                        while((readByte = reader.read()) != -1){
                            StringBuilder sb = new StringBuilder();
                            if(readByte == '\n')
                                continue;
                            sb.append((char)readByte);
                            while((readByte = reader.read()) != '\n'){
                                sb.append((char)readByte);
                            }
                            String line = sb.toString();
                            if(line.isEmpty())
                                continue;
                            String[] splitLine = line.split("\\s+");
                            int size = Integer.parseInt(splitLine[2]);
                            byte[] bytes = new byte[size];
                            reader.read(bytes);
                            writer.write(line);
                            writer.write("\n");
                            ByteBuf buff = Unpooled.wrappedBuffer(bytes);
                            writer.write(Bedrock_v389.V389_CODEC.tryDecode(buff).toString());
                            writer.write("\n");
                        }
                        writer.close();

                    } catch (IOException ex) {
                        ex.printStackTrace();
                    }
                }
            }
        }
    }
}
