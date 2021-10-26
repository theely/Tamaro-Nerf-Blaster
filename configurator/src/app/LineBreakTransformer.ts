export class LineBreakTransformer {

   chunks:any;

  constructor() {
    // A container for holding stream data until a new line.
    this.chunks = "";
  }

  transform(chunk:any, controller:any) {
    // Append new chunks to existing chunks.
    this.chunks += chunk;

    // For each line breaks in chunks, send the parsed lines out.
    const lines = this.chunks.split("\n");
    this.chunks = lines.pop();
    lines.forEach((line:any) => {
      controller.enqueue(line)
    });
  }

  flush(controller:any) {
    // When the stream is closed, flush any remaining chunks out.
    controller.enqueue(this.chunks);
  }
}
