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
    return new Promise((resolve, reject) => {
      var xhr = new XMLHttpRequest();
      xhr.open("PUT", url);
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


class StyleUtils
{
  static greenRedGradient(proportion)
  {
    let rFactor = proportion;
    let gFactor = 1 - proportion;
    let bFactor = 0;

    let norm = Math.sqrt(rFactor * rFactor + gFactor * gFactor + bFactor * bFactor);

    rFactor /= norm;
    gFactor /= norm;
    bFactor /= norm;

    return `rgb(${rFactor * 255}, ${gFactor * 255}, ${bFactor * 255})`;
  }
}
