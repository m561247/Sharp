
import java.io.ByteArrayOutputStream;
import java.io.ObjectOutputStream;
import java.io.ObjectInputStream;
import java.io.ByteArrayInputStream;
import java.io.IOException;
import java.io.Serializable;


class parent implements Serializable {
	int parentVersion = 10;
}

class contain implements Serializable{
	int containVersion = 11;
    SerialTest test;

}
public class SerialTest extends parent implements Serializable {
	int version = 66;
	contain con = new contain();

	public int getVersion() {
		return version;
	}
	public static void main(String args[]) throws IOException {
        final long startTime = System.currentTimeMillis();


        for (int i = 0; i < 1000000; i++) {

            ByteArrayOutputStream bos = new ByteArrayOutputStream();
            ObjectOutputStream oos = new ObjectOutputStream(bos);
            SerialTest st = new SerialTest();
            st.con.test = st;
            oos.writeObject(st);
            oos.flush();
            oos.close();

            if(i == 0) {
                System.out.println("sz " + bos.toByteArray().length);
            }

            // ByteArrayInputStream bis = new ByteArrayInputStream(bos.toByteArray());
            // ObjectInputStream in = new ObjectInputStream(bis);
            // try {
            // Object copied = in.readObject();
            // } catch(Exception e){}
        }
        final long endTime = System.currentTimeMillis();

        System.out.println("Total execution time: " + (endTime - startTime));
	}
}