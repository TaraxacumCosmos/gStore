package jgsc;

import java.io.*;
import java.net.*;
import java.net.URLEncoder;
import java.net.URLDecoder;
import java.io.UnsupportedEncodingException;
import java.util.List;
import java.util.Map;

public class GstoreConnector {

    public static final String defaultServerIP = "127.0.0.1";
    public static final int defaultServerPort = 3305;

    private String serverIP;
    private int serverPort;
    //private Socket socket = null;

    public GstoreConnector() {
        this.serverIP = GstoreConnector.defaultServerIP;
        this.serverPort = GstoreConnector.defaultServerPort;
    }

    public GstoreConnector(int _port) {
        this.serverIP = GstoreConnector.defaultServerIP;
        this.serverPort = _port;
    }

    public GstoreConnector(String _ip, int _port) {
        this.serverIP = _ip;
        this.serverPort = _port;
    }

    public String sendGet(String param) {
		String url = "http://" + this.serverIP + ":" + this.serverPort;
        String result = "";
        BufferedReader in = null;
        try {
            String urlNameString = url + "/" + param;
		System.out.println("request: "+urlNameString);
            URL realUrl = new URL(urlNameString);
            // 打开和URL之间的连接
            URLConnection connection = realUrl.openConnection();
            // 设置通用的请求属性
            connection.setRequestProperty("accept", "*/*");
            connection.setRequestProperty("connection", "Keep-Alive");
            connection.setRequestProperty("user-agent",
                    "Mozilla/4.0 (compatible; MSIE 6.0; Windows NT 5.1;SV1)");
            // 建立实际的连接
            connection.connect();
            // 获取所有响应头字段
            Map<String, List<String>> map = connection.getHeaderFields();
            // 遍历所有的响应头字段
            for (String key : map.keySet()) {
                System.out.println(key + "--->" + map.get(key));
            }
            // 定义 BufferedReader输入流来读取URL的响应
            in = new BufferedReader(new InputStreamReader(
                    connection.getInputStream()));
            String line;
            while ((line = in.readLine()) != null) {
                result += line;
            }
        } catch (Exception e) {
            System.out.println("error in get request: " + e);
            e.printStackTrace();
        }
        // 使用finally块来关闭输入流
        finally {
            try {
                if (in != null) {
                    in.close();
                }
            } catch (Exception e2) {
                e2.printStackTrace();
            }
        }
        return result;
    }

//NOTICE: no need to connect now, HTTP connection is kept by default
    public boolean load(String _db_name) {
		boolean connect_return = this.connect();
		if (!connect_return) {
			System.err.println("connect to server error. @GstoreConnector.load");
			return false;
		}

        String cmd = "load/" + _db_name;
        String msg = this.sendGet(cmd);
        //if (!send_return) {
            //System.err.println("send load command error. @GstoreConnector.load");
            //return false;
        //}

        this.disconnect();

        System.out.println(msg);	
        if (msg.equals("load database done.")) {
            return true;
        }

        return false;
    }

    public boolean unload(String _db_name) {
        boolean connect_return = this.connect();
        if (!connect_return) {
            System.err.println("connect to server error. @GstoreConnector.unload");
            return false;
        }

        //String cmd = "unload/" + _db_name;
		String cmd = "unload";
        String msg = this.sendGet(cmd);

        this.disconnect();

        System.out.println(msg);	
        if (msg.equals("unload database done.")) {
            return true;
        }

        return false;
    }

    public boolean build(String _db_name, String _rdf_file_path) {
        boolean connect_return = this.connect();
        if (!connect_return) {
            System.err.println("connect to server error. @GstoreConnector.build");
            return false;
        }

        String cmd = "build/" + _db_name + "/" + _rdf_file_path;
        String msg = this.sendGet(cmd);

        this.disconnect();

        System.out.println(msg);
        if (msg.equals("import RDF file to database done.")) {
            return true;
        }

        return false;
    }

    public boolean drop(String _db_name) {
        boolean connect_return = this.connect();
        if (!connect_return) {
            System.err.println("connect to server error. @GstoreConnector.drop");
            return false;
        }

        String cmd = "drop/" + _db_name;
        String msg = this.sendGet(cmd);

        this.disconnect();

        System.out.println(msg);
        return msg.equals("drop database done.");
    }

    public String query(String _sparql) {
        boolean connect_return = this.connect();
        if (!connect_return) {
            System.err.println("connect to server error. @GstoreConnector.query");
            return "connect to server error.";
        }

		//URL encode should be used here
		try {
		_sparql = URLEncoder.encode("\""+_sparql+"\"", "UTF-8");
		}
		catch (UnsupportedEncodingException ex) {
			throw new RuntimeException("Broken VM does not support UTF-8");
		}
		String cmd = "query/" + _sparql;
        //String cmd = "query/\"" + _sparql + "\"";
        String msg = this.sendGet(cmd);

        this.disconnect();

        return msg;
    }

    public String show() {
        return this.show(false);
    }

    public String show(boolean _type) {
        boolean connect_return = this.connect();
        if (!connect_return) {
            System.err.println("connect to server error. @GstoreConnector.show");
            return "connect to server error.";
        }

        String cmd;
        if (_type) {
            cmd = "show/all";
        } 
		else {
            cmd = "show/databases";
        }
        String msg = this.sendGet(cmd);
        
        this.disconnect();
        return msg;
    }

    private boolean connect() {
		return true;
    }

    private boolean disconnect() {
		return true;
    }

    private static byte[] packageMsgData(String _msg) {
        //byte[] data_context = _msg.getBytes();
        byte[] data_context = null;
        try {
            data_context = _msg.getBytes("utf-8");
        } catch (UnsupportedEncodingException e) {
            // TODO Auto-generated catch block
            e.printStackTrace();
            System.err.println("utf-8 charset is unsupported.");
            data_context = _msg.getBytes();
        }
        int context_len = data_context.length + 1; // 1 byte for '\0' at the end of the context.
        int data_len = context_len + 4; // 4 byte for one int(data_len at the data's head).
        byte[] data = new byte[data_len];

        // padding head(context_len).
        byte[] head = GstoreConnector.intToByte4(context_len);
        for (int i = 0; i < 4; i++) {
            data[i] = head[i];
        }

        // padding context.
        for (int i = 0; i < data_context.length; i++) {
            data[i + 4] = data_context[i];
        }
        // in C, there should be '\0' as the terminator at the end of a char array. so we need add '\0' at the end of sending message.
        data[data_len - 1] = 0;

        return data;
    }

    private static byte[] intToByte4(int _x) // with Little Endian format.
    {
        byte[] ret = new byte[4];
        ret[0] = (byte) (_x);
        ret[1] = (byte) (_x >>> 8);
        ret[2] = (byte) (_x >>> 16);
        ret[3] = (byte) (_x >>> 24);

        return ret;
    }

    private static int byte4ToInt(byte[] _b) // with Little Endian format.
    {
        int byte0 = _b[0] & 0xFF, byte1 = _b[1] & 0xFF, byte2 = _b[2] & 0xFF, byte3 = _b[3] & 0xFF;
        int ret = (byte0) | (byte1 << 8) | (byte2 << 16) | (byte3 << 24);

        return ret;
    }

    public static void main(String[] args) {
        // initialize the GStore server's IP address and port.
        GstoreConnector gc = new GstoreConnector("172.31.19.15", 3305);

        // build a new database by a RDF file.
        // note that the relative path is related to gserver.
        //gc.build("db_LUBM10", "example/rdf_triple/LUBM_10_GStore.n3");
        String sparql = "select ?x where {"
                + "?x	<rdf:type>	<cdblp.cn/class/Paper>. "
                + "?x	<cdblp.cn/schema/property/has_author>	<cdblp.cn/author/wangshan>. "
                + "}";

        boolean flag = gc.load("db_cdblp");
        System.out.println(flag);
        String answer = gc.query(sparql);
        System.out.println(answer);

        answer = gc.query(sparql);
        System.out.println(answer);

        sparql = "select ?x where {"
                + "?x	<rdf:type>	<cdblp.cn/class/Paper>. "
                + "?x	<cdblp.cn/schema/property/has_author>	<cdblp.cn/author/yuge>. "
                + "}";
        answer = gc.query(sparql);
        System.out.println(answer);
    }
}
