#include <WiFiClient.h>

WiFiClient client;
uint16_t port = 7777;

String request(String method, char server[], String path = "/", String body = "") {
  if (client.connect(server, port)) {
      client.print(method);
      client.print(" ");
      client.print(path);
      client.println(" HTTP/1.1");
      client.print("Host: ");
      client.println(server);
      client.println("Connection: close");

      if (method == "POST" || method == "PUT" || method == "PATCH") {
          client.println("Content-Type: text/plain");
          client.print("Content-Length: ");
          client.println(body.length());
          client.println();
          client.print(body);
      } else {
          client.println();
      }

      client.read();
      String result;
      while (client.available()) {
        char c = client.read();
        result += (String)c;
      }
      int index = result.indexOf("\r\n\r\n");
      result = result.substring(index + 4);

      return result;
    } else {
        Serial.println("Error on request!");
        return "";
    }
}
