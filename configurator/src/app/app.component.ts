import { Component,OnInit } from '@angular/core';
import { LineBreakTransformer } from './LineBreakTransformer';



@Component({
  selector: 'app-root',
  templateUrl: './app.component.html',
  styleUrls: ['./app.component.css']
})


export class AppComponent implements OnInit{
  title = 'configurator';
  reader: any;
  writer: any;
  isConnnected=false;
  configuration: { [key: string]: any } = {
               'pusher_pull_time': 60,
               'pusher_push_time': 55,
               'esc_max_power': 1650,
               'min_rampup_time':140,
             };

isSingleShot:any;
isBurstShots:any;
isFullAuto:any;

 ngOnInit() {

 }

 //Web serial doc: https://web.dev/serial/

async getVersion(event: Event) {

    await this.writer.write("version \n");
    var message = "";
    while (true) {
      const { value, done } = await this.reader.read();
      console.log(value);
      message+=value;
      if (done) {
        this.reader.releaseLock();
        break;
      }


    }
    //console.log(message);
}


async pushConfiguration(event: Event) {

  for (var key in  this.configuration) {
    var command:string="set "+key+"="+this.configuration[key]+" \n";
   console.log(command);
   await this.writer.write(command);
  }

}

async connect(event: Event) {
  let webSerial: any;
  webSerial = window.navigator;

if (webSerial && webSerial.serial) {

    // Filter on devices with the Arduino Uno USB Vendor/Product IDs.
    const filters = [
      { usbVendorId: 9025 }
    ];
    // Prompt user to select an Arduino Uno device.
    const port = await webSerial.serial.requestPort({ filters });
    //const port = await webSerial.serial.requestPort();


    const { usbProductId, usbVendorId } = port.getInfo();
    console.log(usbVendorId);

    await port.open({ baudRate: 115200 });

    const textDecoder = new TextDecoderStream();
    const readableStreamClosed = port.readable.pipeTo(textDecoder.writable);
    this.reader = textDecoder.readable.pipeThrough(new TransformStream(new LineBreakTransformer())).getReader();

    const textEncoder = new TextEncoderStream();
    const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);
    this.writer = textEncoder.writable.getWriter();

     this.isConnnected=true;
    webSerial.serial.addEventListener("disconnect", (event:any) => {
        this.isConnnected=false;
    });

    //await writer.write("hello");

var message = "";
while (true) {
  const { value, done } = await this.reader.read();
  if (done) {
    this.reader.releaseLock();
    break;
  }
  message+=value;
}
console.log(message);

//const textEncoder = new TextEncoderStream();
//const writableStreamClosed = textEncoder.readable.pipeTo(port.writable);

//reader.cancel();
//await readableStreamClosed.catch(() => { /* Ignore the error */ });

//writer.close();
//await writableStreamClosed;

//await port.close();

} else {
  alert('Serial not supported');
}

}


}
