class RequestUtils
{
  static get(url){
    return new Promise((resolve, reject) => {
      var xhr = new XMLHttpRequest();
      xhr.open("GET", url, true);
      xhr.onreadystatechange = function(){
        if(this.readyState !== XMLHttpRequest.DONE){
          return;
        }

        if(this.status !== 200){
          reject(new Error(`${url} failed with HTTP status ${this.status}`));
          return;
        }

        try{
          let data = JSON.parse(xhr.responseText);
          resolve(data);
        }
        catch(error){
          reject(error);
        }
      };

      xhr.onerror = (error) => {
        reject(error);
      };

      xhr.send();
    });
  }

  static post(url, data = {}){
    return new Promise((resolve, reject) => {
      var xhr = new XMLHttpRequest();
      xhr.open("POST", url);
      xhr.setRequestHeader("Accept", "application/json");
      xhr.setRequestHeader("Content-Type", "application/json");
      xhr.onreadystatechange = function(){
        if(this.readyState !== XMLHttpRequest.DONE){
          return;
        }

        if(this.status !== 200){
          reject(new Error(`${url} failed with HTTP status ${this.status}`));
          return;
        }

        try{
          let data = JSON.parse(xhr.responseText);
          resolve(data);
        }
        catch(error){
          reject(error);
        }
      };

      xhr.onerror = (error) => {
        reject(error);
      };

      xhr.send(data);
    });
  }


  static put(url, data = {}){
    return RequestUtils._send("PUT", url, data);
  }


  static delete(url, data = {}){
    return RequestUtils._send("DELETE", url, data);
  }


  static _send(method, url, data){
    return new Promise((resolve, reject) => {
      var xhr = new XMLHttpRequest();
      xhr.open(method, url);
      xhr.setRequestHeader("Accept", "application/json");
      xhr.setRequestHeader("Content-Type", "application/json");
      xhr.onreadystatechange = function(){
        if(this.readyState !== XMLHttpRequest.DONE){
          return;
        }

        if(this.status !== 200){
          reject(new Error(`${url} failed with HTTP status ${this.status}`));
          return;
        }

        try{
          let data = JSON.parse(xhr.responseText);
          resolve(data);
        }
        catch(error){
          reject(error);
        }
      };

      xhr.onerror = (error) => {
        reject(error);
      };

      xhr.send(data);
    });
  }
}


class MathUtils
{
  static roundPrecision(num, decimals)
  {
    let factor = Math.pow(10, decimals);
    return Math.round(num * factor) / factor;
  }
}


class ColorUtils
{
  /**
   * Converts a streamed color (CIE xy chromaticity and brightness, as sent to the bridge) to sRGB
   */
  static xyBrightnessToRgb(x, y, brightness)
  {
    if(y <= 0 || brightness <= 0){
      return {r : 0, g : 0, b : 0};
    }

    // xyY with Y = 1, then linear sRGB (D65)
    let X = x / y;
    let Z = (1 - x - y) / y;
    let rgb = [
       3.2406 * X - 1.5372 - 0.4986 * Z,
      -0.9689 * X + 1.8758 + 0.0415 * Z,
       0.0557 * X - 0.2040 + 1.0570 * Z
    ].map(value => Math.max(0, value));

    // Full chroma at the streamed brightness
    let maximum = Math.max(...rgb, 1e-6);
    rgb = rgb.map(value => value / maximum * Math.min(1, brightness));

    let encode = (value) => value <= 0.0031308 ? 12.92 * value : 1.055 * Math.pow(value, 1 / 2.4) - 0.055;
    let [r, g, b] = rgb.map(value => Math.round(encode(value) * 255));

    return {r : r, g : g, b : b};
  }
}
