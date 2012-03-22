	// ==UserScript==
	// @name                XIA Inspector
	// @namespace	        http://www.cs.cmu.edu/~xia/
	// @description	        Adds an "inspector" panel to any page containing XIA links. Among other things, the inspector shows a graphical view of a link's DAG when the mouse is held over a link.
	// @include		*
	// ==/UserScript==
    
var css = ".xia-link {text-decoration:underline;color:#090;} .top-right-button {position: fixed; right: 0; top:0; margin:5px;} .ie .inspector {/* IE10 */ background-image: -ms-radial-gradient(center top, ellipse cover, #F5F5F5 0%, #E3E3E3 100%);}.gecko .inspector {/* Mozilla Firefox */ background-image: -moz-radial-gradient(center top, ellipse cover, #F5F5F5 0%, #E3E3E3 100%);}.win.gecko .inspector {/* Mozilla Firefox */ background-image: -moz-radial-gradient(center top, ellipse cover, #F5F5F5 0%, #E3E3E3 100%);}.linux.gecko .inspector {/* Mozilla Firefox */ background-image: -moz-radial-gradient(center top, ellipse cover, #F5F5F5 0%, #E3E3E3 100%);}.opera .inspector {/* Opera */ background-image: -o-radial-gradient(center top, ellipse cover, #F5F5F5 0%, #E3E3E3 100%);}.webkit .inspector {/* Webkit (Safari/Chrome 10) */ background-image: -webkit-gradient(radial, center top, 0, center top, 559, color-stop(0, #F5F5F5), color-stop(1, #E3E3E3));} .inspector { border: 1px solid #CCC; } .persistent-footer { position: fixed; height: 200px; width: 100%; top: 100%; margin-top: -200px;}"
var inspector_image = "data:image/png,%89PNG%0D%0A%1A%0A%00%00%00%0DIHDR%00%00%00%3A%00%00%00%1C%08%06%00%00%000N%AC%03%00%00%00%09pHYs%00%00%0B%13%00%00%0B%13%01%00%9A%9C%18%00%00%00%19tEXtSoftware%00Adobe%20ImageReadyq%C9e%3C%00%00%06XIDATx%DA%E4Xml%13e%1C%7Fz%BD%B6%D7m%D02Z%D6m%EA%5E%A2%BC%2C%98MaD%E4%03%251~%11%84%8F%0A%0BC%9D%12%88%C9%C67%A3%C6%0D%BE%A8%24%C8Pb%22%D10%E2W%0C3%C4%BD%40%B6u%242%12%10%8AX%03c%83%16%D9%DA%AE%AF%A3%EBzw%BD%DE%F9%DC%F6%3F%F2x%DC%D6%EE%05%C4%F8O%FE%EDs%CF%EB%FF%F7%FC_%EFth%9E%24%A4y%3A%12%89Z%FD%81%B1%02!-%E8%8A%1Dv%D6f%B7%C5M%8C9%85%9EB%D2%CDgQhll%85o4%FC%22e4nJ%B1%7Ce%26%23%D2yy%8C_%2F%89%97%ED%16%F3%95%D2%D2R%AF%8E%A2%C4%A7%09(%3D%D7%05%DE%BB%BE%8A%91h%E2%3DV%A4v%F3%BC%EEY%3DcE2%A2(%C7!%3A%93NPt%FA%2Cw%D7%7BL%CCd~%A3%F4%FA%CC%7FR%A3%D1H%A4%E8%5E0%F6U%94C%3B%ED%8Ebd%C93%23%23%25M%8Dq%18md%82G%B1p%10-%A7%D3%EEb%9B%E5%FD%A2%A2%15W%60%A9%05s%8DrW%98%7D%B3%F4o%D6%98%A7%D0f%A2%ED%C6%3C%BE%E87%22%89%19%EA%8E%EF%FEg%BD%D7%86%24o%3C%23E%93%82%94b%05)%CDe%A6%98%C5%EDX2%23y%C73%D2%AF%9E%7B%D2%E0%B0%EF%3C%CFqK%09%401y%1B%CC%7D%C4%B6%CD%D0's%99r%14p%B3J%84jbL%E6%FA%B9%C8O%E5%3A%91%E3%F8%A2%E0%F8%E4%3EGq1Zj%90%D0%12l%F4%0C%ADC%B4%1EM%B1%09%B7%97%180%22ZB%CF%948P%2C%25%BC%96N%A7%B7%C0r%F9%E6%9B%A0%ED%04%CDX%88%BEV%0D%ED%A9i%8F%EAy%C7c%01*%08%C2F%81b%1C%CB%F2%CD%C8D%C9%E0%1E%B5z%3D%EEg0%2Fe%0C%88f%F2%10%CB%A7%DF%20%86O%81%B9!%00(%0Bj%C5%1C%C7%DC%92%83%08N%F8o!%80Z%16%1DhF%94%CAi%A3Q4%60%ED%19%A8%99%5D%5B%D6%AE%7C%07F%A3Q%CAH%0F%CD%11%11%00%15!%5B%08%C1%B3%F9Z%19%E1%CB%075%C0%2F%1EP%9A%D6%87%B1%FC%82%EC%1D%92%24%CD%E2%CC%08%89x%1C%83%14%0D4%3D%A2%1A%ED%C7%EC%82v9%04%9Cc9%1C%AF%98%A9%B2%B6%7D%AE%E6%9B3P%03%AD%F7%8B%02%AF%E7%04%11%A5E%19%AC%06F%DC'%60f%D3%12J%A68%CAHS!%AD%0C5%8FX%E8T%01u%3D6%A0%B4%5E%7F%C3%84%04o%241%89XI%87xYe%18%99%24M%03%141%A7%F1%8F%9CfX%8EE%26%91K%D1z%EA%82FzP%82J%1C%B4%DA%98%E5h%0B%01%A8%05%22n%2B%3C%5BU)g%E1%40)%DA%10X%C6P%5DA%FF(%8AO%F2h%9CG(%25%20%C4%8B%D3%2C%B7%E3%B8o%9C%15%D0h0%94)%CC%D3%F7%18%8C%A6%5E%D56%8A_%B6%11%C2%B6d%09*%A4%1F%BA%08%9ES%F4%CD%A92%8A%C5b%8C%C7s%F3%B9%3F%3C%9EJ%C6%5C%20%DD%1F1%24%0Bm%B6%02%B3%C98%15%98%E4%2B%C6%A9%14M%B2%3CJ%C4%A3%A9%02)%F5%7B%A9%A3%F2%13%8A%A2%C8%BA%B7%5E%159%E3%10%9C%AC%F0%7C%20%8B%7F%CA%11%7B%0B%D1%7F%06%C6v%CC%B26w%8D%0E%0E%0EY%7F%F8%FE%E4%D6%07%0F%1E%1C%09%87%23%AF%17%98%8D%EE%C1%CB%AE%5BW%2F%5E%08%8E%8C%06%02%7F%8DE%12%BE%40%24yo4%10%0E%8D%F8n-%11'%CE%AEy%BEl%AF%89an%A8%CC%AF%85%D0%8A%0F%22m%2B%11%8D%AB%B3%00mW%F5%93A%ADzA%1A%1D%1E%BEc%ED%F8%A5%B3%EE%D2%A5%81%CF%CD%F9%F9%05%1Bjk%BD%0EG%D1%BEW7m%FC%A2%AA%AA%EA%EC%E9%D3%3F%0D'%26%E2%15r%AC%B21F%7FQ%E5%8A%81e%CBm%3D4M%AB%DF%60%CA%C1%5C%D5%02%B7%AA%E6%5CW%5DH%191%A7M%B5g%3BX%C3%C2hh%E8%8E%E5%F8%F1o%3F%DC%B6%F5%CDd%ED%BAZ%A9%B9%F9P%A2%A7%A7%F7c%18%EE%D3(%D1%9E%245%CF%F5%7Cz%26%90%1D%1D%1D%BB%CE%9F%3B%F7e%C0%EF%CF%7Bi%DD%FA%09%861%B5W%ADY%7DBcz5%A4%0C%A5%D2i%23%0A%802%88%B2%5E%A8%8C%D4s%D59%B4%1E4%DB%AA*%22%EA%09%AB%B0%C2z7D%DC~p%8D%3D%E0%F7%A7%08w)%87%3E'%F5(%C8aKgG%A7%0C%F2%B0%02r%D5%AA%95%ED%0D%0D%EF6%16%97%94%845%80%B6%C2%A15pX%1B%01R%C9%995%C4%5C%171%F7(%B1%CFI%00PC%94%8A2%5D%23%FC%D4%0A%EB%CA!%B0)%11%DB%0B%7DM%84%A6k%40%167q%FE4%DD%BE%3Dl%F9%E6%EB%E3%FB%B1%B9N%C8%E6%BA%F7%83%FD%89%23G%8E%FE%18%0A%85%0AU%E0H%D3%ED%03!e%DA%0EyN1%AF%3E%8DuZs%CB%E0%EDFI3wA%5B%F5%D0F%B3%9C%DFH%9C%B3%1D%F6Qr%B6%A4%E4i%8A%04%D9%D9%D9YwnZ%93%F9%B2%26WbM%EE%DE%5D%D7h%B7%DB%A3%D9%DE%C7%89%22%80%8C%8AN%8D%D7)e%EE%CF%84%E9%3B%A1%BF%06%04%8CC%9FS%23%DAj%E5Y%17%B1%A7U%15%85%8F%3D%F4Q%8F%E7%CF%C2%AE%CE%AE%B7%CFwc%90%01%D9%5Ck1%C8%170%C8%5D%B9%80%9C%89%FAU%A6%7Cj%86yV0%BBr%22%E2%C6%01%B8SU%1C%CC%B4%3Ek%DF%14%D0%81%8B%03ouww%1F%0Eb%90%2F%3F%04Y%B7%10%90%E4%AB%99W%A3%9A%D1%22uA%80%88%A0%B5%60%9A2%DD%ABW%AFm%F1%8F%8E%D0%EB%D6oHbs%3D%B3H%20I%CD*y%92%BC%EDjb%BC%1D%B4g%D1(%0A%B2%95x.%22%D8T%AB%CE%FC%A7F%1B%1A%DE%F9%14'yGEeE%A4%AEng%D3%22%81TR%82%12E%FB%89%2F%05.%95Y_'%B4%AE%14%02%07%60%BC%09%CA%3D7%8C%5D%07%2BQ%D2V%1B%FCo%87Ki%D3%12F%2F%FF%7Cw%E2D%E4%E0%A1%83%DE%92%12G%EF%DA%B5k%87r%04%E2%26%3E%7F%90%ED8%01j5%E6%9B%20%2C%07%C2%5D%C2%FC%0AQ%EBr%84%99%B3%00%F2%26%08%3FN%E4N%06s%17%CCw%C1%5C%16%CEr%C3%5E%F2%9A%8F%88%3D%E3Z%DA%7D%12%F4%AFUT%14%FA%9F%D0%93%06%EA%9E%E7%17%86%05%D3%DF%02%0C%00%ED%91%9E%1F9%B0d%13%00%00%00%00IEND%AEB%60%82"

var inspector_title_image = "data:image/png,%89PNG%0D%0A%1A%0A%00%00%00%0DIHDR%00%00%00%A7%00%00%00(%08%06%00%00%00%9B%F0%EA%E1%00%00%00%09pHYs%00%00%0B%13%00%00%0B%13%01%00%9A%9C%18%00%00%00%19tEXtSoftware%00Adobe%20ImageReadyq%C9e%3C%00%00%16%E8IDATx%DA%EC%5B%F9s%5CUv%BE%BD%AB7%A9%25Y%F2%22%D9n%DB%B2%BC%CAn%036%18%C3%20%03%C6%98T%C6J%F2%C3T~%1Ae%F2%07%20*%95%84%AA%04%90l%AAp(R%25RSE%F1%C3%D4%88J%25C%92%A9%19%91%C9%0C%A9P3%C8l%B60%D6%E2%0D%2F%92-Y%1B%92%B5tk%E9%96z%7B%F9%BE%A7%7B%9B%E7%A6%5B%12%5Ep%8D%E9%5Bu%F5%5E%BFw%DF%5D%CE%FD%EE9%DF9%F7%CA%24r)%97%EEB%D24%CD%14%0A%85%3C%D3%D3%D3%85%93%93%93%DEH%24%E2%D0%92%9A%D9f%B1%24%DC%5E%CF%9C%D7%E3%99t%3A%9D%13V%BB%3D%ECr%B9%B4Lu%98rb%CC%A5%3B%0CJ1%3E%3AZ4%3C%3C%B2.%9A%D0%1EL%08%B1%3B%1A%8Bo%8A%C5%13%25%00%A7%CDb6%CF9%1C%B6%11%87%D5r%DEf6%9Dr%BA%F2%3A%DD%1E%CF5%B7%DB%3D%99%0E%D2%1C8s%E9%8E%A5p8l%1B%1A%1C%5C%1F%8ED%FFt*%3C%FB%97S%B3%B1ms%9A%D9%91%B48%84%B0%DA%85%26%CCBK%C6%85%88G%855%19%15%5E%BBi%AE%C0%E3%FC%DC%EB%CA%FB%85%D7%E3%FE%00%20%ED%F5z%BD%B1%1C8s%E9%8E%A6%B9%B99%C7%40_%FF%9E%89%A9%99%BA%D1P%E4%40%D4%EA%F2%BA%8A%96%09%B77_%D8%ED6a6%01%98(%97Hjb.%9E%1033%111%13%1A%17%96HP%14%3B%CD3%CB%0A%3C%FF%5E%90%EFy%DB%E9r%9D-((%88%A6%C0Y__%FF%3C.%BE%B4%F6%9A%F1%BC3%BD%13x%B6%13%97%9A%B4%C7A%3C%7FS%BE_%8BKm%DA%FB%26%3C%EF%CD60%BC%2B%C0%A5.%CB%BB%86o%23%24%94%7Fe)m%E6%D2%1D5%E5%D6%C1%BE%BE%C7%06F%C6%8E%8C%CC%C4%1F%CE%2BZa_%5EZ%22%9C%04%A5H%0A%2BPf1%CD%83%0DV_%40w%8Ah%D2%24%A6q3%11%9A%163%A3C%A2%C04%AB%AD(%F2%FE%AA%A0%20%FF%98%CB%E5%E9%F4z%5D1%2B%2B%F7%FB%FD%7F%96H%24%9E0%99L%82%99%BC!%18%0CV%E3%D5%FE%F4%8E%00%D5%8D%C8%D5%AA%1C%B3%DDn%7F%0F%AFtp%96%97%97%FB%CDfs%3D%DF%2B%0E%82%DF%9F%E06%2BP%C07j%96-%5BV%8Fr7%3DO%26%93%E2%D8%B1c%CD%2F%BE%F8b%E7R%05UVVV%AF%AF%3A%93%E9%A3%85%DA%CC%A5%3B%97%82%E3%E3%9BG%C6%82%7F%3F%1CN%EE.%5C%B9%D6%BE%B2%A4P%07%A4%0D%C0t%00aV%60%C1%2C%C1%99%94%DA3%8E%9C%87%E9%F6%14%7B%C4%B4g%9D%18%1E%E83%8DN%86%FF%C2f%B3OX%AD%D6%7F%82%03uU%07g%20%10%A0g%25%2C%16%0B%81%A4%83b%7C%7C%BC%1A%9A%E7%09%E4%E3%06%AD%F4%C4%86%0D%1B%AA%8B%8B%8Bu%10%B3%5C%3C%1E%17%2BV%AC%B0%A92%EB%D6%AD%13yyy%02%0D%E8%BF%F9%1E%E5%5C%0B%0Dn%D5%AAU5%15%15%15%C2f%B3%E9%7D%60%C2b%11%B1X%8C%EDP%0B%BF%B0TA%AD%5E%BDZ%AF%03%DF%D9%BE%2F%E0%A0%E5A%0E%DD%8B%B6%01%22%CF%E0%C0%D0_%7F5%15%DB%93_Z%96W%5E%3A%0FL%3B%80%97%07u%C9%2B%B5%A6Y)%2B%02%94%DA%13(%B5%C1%5B%B2%244%E1pY%85cM%B9%E8%EF%ED%15%93%E1%D9%1F%C1aj%07%15%F87%1DA%3Bv%EC%F8%C3%A9S%A7~%40-%C7%89%E5%15%9A%8CZ%88%C0H%81%B3%A4%A4%A4%8E%CF%09%3C%82%93%C0%83%D6%13%95%95%95%FF%A9%CA%F0%7B%82%8CYj0%02%CD%B2%D0%00A%82%AB%1D%0E%87%606%82%93%0B%05%ED%D5%7C%1Bp*%EDoNW%C3%F7)(I%87%203%CE%D3%BA%7B%D1%87%E8%DC%DC%DE%E1%F1%C9%83%C2%5DXT%B6%AC%40%D8%20%7B%9BY%13.%20%D2%81%A9%B4%989%1F7%7F%03%D5%A1%03%D6b%D2%F4%B9%9A%89k%A2%D8m%17%91%D2%E5%224%3A%E4%CD%8F%C5%7F%94%17%8B%9D%D0'%10%F3%D8%00%8D%D3%23M%B0%0E%3E%82%AB%B4%B4%B4VrH%9DKR%C3%F19%DF%AB%B9%C7w-%B8%7FG5%CC%E7%04%18%CB%A8lQ%88%CB%90%5E%7D%F5%D5%C3%D0%BC%3EP%83%14%A8%99%F9%9B%19%E0%F4%BF%F6%DAk%3B%BF-8M%A6%FB%DF%D7%F3%F9%7C%BB%20%FF%FA%9D%3Bw%FA%EF%15%D7%9C%99%9E%3E%14%8A%5BV%94%2C%2B%D49%26%B5f%DE%02%C0%FC%1A'%D0%9C(%97g%11z%A6%3A%5DU%9C%2F%12V%A7%98%9DK%ECN%C4%93%BBR%DA%05%00%A9%A3%164j%3F%3C%238%EA%A5%20%EA%01%CE%140%09d%3C%13%85%85%85u%E9%E0P%00U4%C1%B4%00R%C0wk%08B%09b%BD%5DU%07%9F9%9DNA*q%0B%B2%BB%EF%D1%09P%E6A~%3A%8D%BA%17%09tm%F9%E4L%24%60r%B8%BC%3E%B7C%10L6hC%87ya%60~%8D%15%F0Q3M%3F%CA%C2%D4%BB%80l%8F%C7%23f%E3I'~%EE%B1%1A4%DE%7Bccc-%5D%5D%5D%D5%0AX%04%0D8%24%B5g%E3%9A5kjhv%15'%258%F1%AC%09%E5%3A3%81S%E1Qj%B1%AC%DDDg%AA%8D%E5q%FF%2B%E4%3Fg%1B%0A%E4h%B7V9%5C%B7%AA91%86%0Fq%E9%C0%F5%05%E9%D1%B3Nj%9C%16%9A%C6%2C%91%09c%B9%1E%19%C1x%C1%10%B5hd%9D%FC)%EFU%94%A2I%D6%19%CAb%8A%1Be%C4%C3%B7%84%B2u%86%3E%04%E7%1F%D7%BF%C9%F1%5C%BAt%A9x%CB%96-%C6%F1%095F%A3%9F%20%FBW-%BFo%CE%D4%9E%FA%1E%D7%FD%C8%87e%1F%9B%16%8A%96hZrC8%96%2Cu8%DDV%3B%95%16%10f%D5%81%B9t%80S%83%EA%DF%D0%C4%03%91%5E%B7K%84%22Sp%9CL%5B%AC%C6%82EEEu%F0%C4%3B%40ru%60%10%8C%00%A0%18%18%18h%82%F6%F2Q%AB%11%94%E4%9A%D0%98A%97%CBU%B7%10%40%D4%7D6-%C6%09F%7B~%E5%5C%91g%E2%FAk%B4%BD%1Em%04%14M%40%5B%01%D2%8A%5B%0D%0D%A1%FF%D6%13'NT%CF%CD%CDU744%F8%D6%AE%5D%5BK%EE%CC%F1MNNV_%BBv%AD%05u%FB%8D%13%86r%3F%87%C9%AC%05%CF%D6%CBA%26%FE%89%89%89%94sVUUU%02Y%E8Q%8B%CB%97%2F%B7%40%83%05X%96%F2%19%19%19%A9E%9D%EC%FF%AEt%B0a%C1w%A0%AC%1F%B2%D6%17%DE%8D%1B7X%96%CEg%C0%D8%3E%CB%C2r%B4%C0Z%05%40%AF%F4%3E%40%3E%BE%D9%D9%D9%CD%18%8F%B3%B5%B5%B5%1A%F2OY2%F4%A7%9A2D%B6%18%EA8%9C%9F%9F%DFL%ED%0A%5E%AF%7F%3F%3A%3AZ%DB%DB%DB%CB%B6%AAU%7BJ%3E%1C%CB%D1%A3G%7F%82%BE%FD%8C%8E%25%E6%DB%BA%B0%22%D0V%C65S%9E%DDf%D3%1D%1Eb%D2%9CR%0E%DF%02%A0%A6%F9Lw)%CFn%15A%93Y%98-%E6%12k%1A%A8%3A%83%C1%60Sww%B7%AE%018pj%CFG%1F%7D4%40%D3%C1%C1%D3%83%A600%E0F%94%0Fe%02f%FA5%9B%E2%5C%B9re%8D%A2%12%12%98t%BA%3E%0E%85B%0F%A2%9D%80%D2%A8%2C%03%40q%E5%BFs%8B%16%C8N%E0%B0%3E%D0%84Z%02%83%20%92%0B%92%13%E7%3Bw%EE%5C%8D%AA%1F%13%B4%13N%5E-A%C1o%D8%3E%17%2ChLj%20%C5%C5%C5v%9D%CC%CF%CCT%EF%DA%B5K%A7%1F%AAN%D2%1F%00%87%00%F81r%AA%CF(%D3H~H%A0%11X%1C3%17%3C%EA%F5wvvr%A1%A7%B4%14%E4%DE%04G5%A0%E4%A3%9CU%8E%03%C9atL%99%F8%9Bs%83%3A%AD%0A%DC(%DBD%CD%CA2%00)%03%E5%3A%05%E0b%3Fs%E6%8C%B1%3D%1B%BFg%FD(%F3%2F%EC%3F%FB%B6%98%23k%A6%5Bc2G%CDfS%82%FE%0Du%D0%ADP%7D%934%F1Bz%F5%26%B3y%0Er%8FX3%C41%EB%D1%F9%9Ap8%ECS%BC%8F%3C%C0%10%16%12%CB%97%2F%EF%C1%F3%86%85L%AB%F1%9A-%B1%1D%15%1D%A0%60q%DF%83%F6%7Bq%DFr%E1%C2%85%3AN%A2A%7B%D6%DC%068%CD%0A%E8%9C%84%AD%5B%B7%B2%3E%01M(%00J%1D%20eeeO%AA%FA%E9%F8%11%98%EC%1B%00%12%A45%C1%B8%FDX%B8ACT%C2%CC%FA%C8%BB%996m%DA%A4%DF%F7%F5%F5%09h%26%FD%BE%BC%BC%BCV%D5%F9%FA%EB%AF%17%C0%FA%D4%12%1C%D4J%A0K%1D%D1h%D4%07%90%F8%A5cY%AB%C0B%8BRQQ%A1%2F%5C%CA%80u%B3%BF%94%11%BE%B9%08m%3F%B5o%DF%3E%BD%FF%D0%DA%FA%9C%3C%F6%D8cJ%DEQ%15%3B%C6%02%D3%E7p%E3%C6%8DT%04-%F8%D6%AF%DA%C3x%8D%E04%B1%1C3%9E%BB%A9%90%D0w.%A6%F8%22R%BDn%B5%98%87%A2Im%3D%96%8F%0EN%7D%7D%EA%7FLKf%FD%9A%84(CL%D1%84N%E7%26a%E4%3B%CD%19%80%D5%0BG%A8Q%AD%D64%EFLy%F1uK%E1~%8B%84A%D6b%D2%03%AA%1C%81O3%A6%2Fc%9B%ED%3Di%C6%F4z%08%12h%9A%1A%C9%C1n%25%A5h%2Fc%B4%C8%E4%D5%26%5E%D5%C2%C3u%83!V%AB%C7e%09%06%B9%E1%B0%1F%E5%D7A%CB%D6%1A%D0%9E%AA%13%13%1F%C4%3B%BDN%D0%20%3D%DC%26%17T%B5%81%5B%D7%B0m%3EG%FD%DCp%D8%851%06%F0m%87%5C%A8~5%3EFIX%07%134_%0F%EBf%7B%00%8D%0F%F54%01%40%09%B6%05%60%1E%A6%DC%98%F9%9Be%90%AB%25%F8%F4%C8%0A54%DA%A8%C5%F3%FD%C6%F6%B0%00%7D%B4%10*%5C%A96T(%EB%ED%DB%B7%F7pa%A1%BF%3F%5Bl%D7%D2e%B7xb%B1%B85%81o%F5%00%BB%5E%8F%02%DCR%3C~~3%FF%DDLL%137%A6%E7%B8%AB%94%8F%A5%EF%CDH%5D%C1g%9A%C0%852%82%13%02%09b%00%EF%DD%AE%A7%C7%F8%25%B5%8B%D2%9C%04%22%16E%8Ba%F2%9B%15%C8%A9%BD9%B1%98%A4%9A%DBm%17%13%D4%84%3A%8F%CB%BA%8Fc%02%5B%D8%3E4%9A%CDPFo%17%FC%8E%DC%B3%CEH%7B2E%03%D0%B7zU'%FA%FD%26%40%F4%AF2%C2%A1%B6s9%B6%DDl%07u%07%19%BA%93%F5%85%00%1E%7D7%8D%20By%9Dg%C32%E9%8B%96%DA%12%8B%A3%C6%D0%DF%90%91J%01%94%09%C5%D53%C4%8E%03R%83v%A8P%1F%BFE%3F%EA%95LA%95%02%12%C8%1A%01%CEz%E8K%D0IE%99w%A8%A8%16%11%E7%B8%DBn%8EFg%C3b6%1A%D7%01%19%D7%E6sJ%81.%E6%F1%D3%87IJ%D3%8E%CC%3Dwx%EF%0E%A0b*%238O%9E%3CYK%0E%95I%FBAp%BE%D7%5E%7B%ED%F9%DB%05%09%F8P%B5%22%F3l%07%26G%BC%FD%F6%DB%F4%22%3Fd%FE%E8%A3%8F%02%C6%90%14%CB%82%E7V%DFn%BB%E0%B3%1F%18%7F%C3yiI%2F%03%F3%3B('%96%26%91%D1%8Av%05%B2%2C!%95_%DEDp%ED%F6%F7%95%C6%07%00%FC%92%06%E8%40%00%5D%F1%A1.M%E5%B7%DEz%2B%B5%08%B7m%DB%A6%16f%B5%04_3%EE%3B%17%0E5fF%00%16%9Br4%9B%D3%C6%FB%3F*%F4Dg%D4%A8x%08t%CC%7B%D3%12%40%A9R%7F%9E%DD%DEi%89%CDN%8FC%E3%25%240%E7%12%F3%5B%94%84hV%80j%F3%3BE%B1%24%B3%A6%87%94%D0%01a%8D%85%85%D3f%9A%C5%87%A7%CC%99%CC-R%1DW%B2%EA%B4%EA%B8Z%E1%00I%FDb%26V%5B%60%D9%F0%5B%9Ai%15%07%E5%95%8Ba%CF%9E%3D%81%C7%1F%7F%BC%9A%09N%86_%81W%81%13B%BDm%CD%09m8%B5%84%85%D3L%CE%25%B5%8Ax%E0%81%07%E8%10v%C8%F0%D17%12%9C%A2%A94%AB%D0%A1%00%A7%EA%81%26%B3K%93.%C8%17%F7%EE%DDKGS%CFJ%C6Jf%8Aj%90%EB.%12g%CC(kx%DF.%F2F%B6%0F%07%E7%92%F1%1D)%81z%A6%9C-c%1D%D7%AF_%FF%E0%5Bp%A5X%9E%CB%F5Aq%9E%E8%1F%9F%98HN%CFF%19%02%02851%9B%98%D7%88%BA%89%D7%BE%D6%A2%EA%9E%1A3%C6rq%B9%A5I%A4%8FN%0A%BB%16%15y6%CB9%93%D9%F4MpB0%F5%EB%D7%AF%F7%A9%F0%0E5%9A%F4%DC%84%8A%3D%F2%3D%CA5.%06L%A3%C0%D3c%9B%D0%C0%A9%20%BD%0A%5B%D1%8C%A9%AC4%B71%A8%8F%E7%BE7%DEx%E3%F0%DD%0E.%A3%AD%5E%F2B%02S%82D%3C%F8%E0%83%3E%00%AC)S%B8%2C%3D%C1o%EA%CBV%86%A0%A1%B9gf%BD%04%22A%C2L%8F%DA(C%80%EC%D4%EDrlX%8A%A1%F4%97%D0%C8%93%D9%7C%0A%B1t%BA%A8%E6%B2e%99%CF%DB%E6%88N%85%07n%04E%24%96%10q%004%02%D0%85%E3B%07*%0Fy%10%8C%D4%A6%E4%96%D4%94%7C%CFm%CB9%DC%C3%F9%11C%A1Y%11%9A%18%15%3E%A7u%D2a%B3%FC%07%E6%E0%9A5M%A3%3D%C1%A0%3B%B5%14%3BJ%60%CA%F0E%07%04%15P%3BC%24%DA%F4%3C%19%9CO%0F%5E%1B%81%99%CD%F6%00%985%F2p%C6M%26E%15S%3CJq)5%C9%D2%AB%A6%F6%BCe%CE%ABi%DAR%01%DA%C0%B2X%10%F5_~%F9%A5%BEx%E0%9C%04%8E%1C9r%F8%E5%97_%5E%B0%FDS%A7NyU%B8%CA(%0F%8E%0B%CF%7B%E0%7D%F7d%F94h%04%FF%F0%F0%F0f%DC%FEn!%A7%93%F5fX%24%9Az%0E%A7%AE4%038%F3%B9H%EED%0A%06g%A2%23%A3%E3%03.17%3B66%E4%E9%B3%C0%E3%2F)%14%0E%CC%2F%81%18e%90%DDxdN%F2R%9A%F3hr%DE%B4OM%CD%8A%81%C1AQd%8D%CFz%5C%EE%FF%05%B1i%86e%1D%BB%09%9C%24%F6%F0%E6R%BB%40%04%07M%14%26%A8%B6%BF%BF%BF%83q2%15%5Bc%2C%0C%80%A5%F6%DC%9F%09%00%99%40%AA%12%C3%1CJ%A0%0A%84%D4%22%EA%3B%05F%F6%81m*%A7%89%CF%E8%B9%DEm%60%1A%26%9F%00m%A9%AA%AAj%3E%7B%F6%2C%AD%059h%40.%8E%14%22%F0%CE%8B%CB%A4%FA%3D99%F9%04%B5%3F%DBc%00%9C%89%FF%82%40%07%AB%BD%BD%3D%B8y%F3%E6%FD%0B%B5KNH%07%05%8B%B1j1%F5%98E%7BG%08%3E*%97%E9%E9%E9%ADi%26%DFt%E6%CC%99M%CAa%BBM%8A%E4ho%3F%F3%F8%F9%0B%17%9E-%C8%CF%2F%2C%2F%2F%13%E3%23%03%E2%DA%5CT%2C%2F)%12N%87%1D%A04%E9%3BFf%D3%D7f%5D%07fb%FE%D0%F1%14%1C%A0%D0%8D%AFD%A1e.R%E4-%F8%CCnw%BE%E9%F6%14%F7%9AdP_i%CD%1F%D3%E10z%CF2%E4%C0%60%7Bgiii%8B%0C%F2%A6%C2%3B%98%A8%EA%A3G%8F%1EN%07%80%DC%A9He%A3%E6%A4v%06%C0%7C%AA%9C%0C!%05%C1s%EB%C9eiNy%E5o%E4%26%82%93%ED*%CE%0B%80%F8a%DAw.%04%C0%0Ct%C2%D8%05-Ky-%0B%00%8E3%9CD%9A!%F9%E87v%C2%AE%5D%BB%F6t%9A%E9%3E%A8%AA%AB%A8%A8%E8%91%11%80%2B2%10%1FX%C8%B9%92%A1%B4%1E%A9%E1j%17%2B%AB%3C%EF4%5E%A9A%5E%7D2~%7C%13O%7F%F7%DDw%7F%A8%C6%02p%F6%2C%22%B7%8C%E9%F2%E5%CB%8E%8E%F6%8EU%00%E6SW%BA%BB%FEattt%0B%14%9A%C5f%B3%C7%8A%ED%89%AF%12%13%03S%3D%3D%3D%C9%81%E1Q16%1D%16%E3%E1%A8%18%85%8D%1F%99%89%8B%A1%E9%98%18%9A%9C%05I%9D%14%7D%03C%224%D8%23%8A%CD%91%F1%12%9F%FB%F7n%B7%F3%95%82%02%F7%17.%97)6%1F%E4%97%0E%0A%40%C8%E0%7B%CAT%104%00B%0F%04%F5%8E4%C5%F5%DC%02S%81x%B5%EB%C0%98%A8%D19R%A03%F2%D4%A4b%EE2%FE%A6L%0A%DF%B3%3E%86%8D%A8%A5%D23%16%C0_!%07%15%EFU%94%02%DA%BCv%C9%B1%DD%0C%DA%7C1%AD*%B7J%D7%1A%B62y%BE5%15f2%02S%EE%3A5*g%89%8B%1C%E3%AB%95%7C%2C%A8%3C_%C8%B7%9DmPf%D0%A2%CD%E9%0E%A5%F17%40%D3%A1%16%02%EAh6%D4%5D%90%FE%9D%EA%C7%B1c%C7%9EO%DB%E0%E8f%7B%8C%25%B3O%EA%7Bn%B2%A8%8D%0DP%B3%8E%C5%E4%96%9E%00%3A%D7%EF~%FB%FE%13%5DW%BA%FF%F6%FA%F5%BE%9F%06'B%FBV%AF%5E%ED())%9E%09%85%82'%E1%C8%FC%B4%BC%D8%FB_E%C9%A9%F3%E1%AF%AE%05%87%AEv%25%06z%7B%B4%C1%BE%3E140%20%BE%EA%EF%13%A3%7D%D7%C4%DC%E0%D5%B8'2%3C%B6%D2m%3AW%BA%AC%F0%E7%3E_%E1%DF%00K'!%CB%A8j%CB*%05%5E%C7%90%87%11%98%BC%02HF%07%E0%F8%C4%C4D%D3%85%0B%17j%D5%C9!f8G%FE%B1%B1%B1%D4n%83%02%A6%D1l%1B%C1%09%80%D7%189%26%DB%C2%84%B5%2C%10%97l%89D%225F%9A%C0%80%B6%C8r%C6%F3NhN%D0%9B%1A%B4I%C0q%F2%82%10%9A%AE99%96%AD%5B%B7%06%8DN%87r%10!%03z%F3%0C%1D%E91Y%8E%8F%BB2%86%08%C0%2F1%CE7%A6%A6%A6XG%A0%B3%B3%93%FF%DA%D2%22%2B%0A%60%9C%F5%EAp%0B%80%DC%8C%FAjh1X%F6%E2%C5%8Bz%DD%F2%08b%BD%925%E8A%84%7C%98%FD%90J%A2Q%1E%0Ci(%2F%2Fo%BEq%E3%86%BE%FF%8E%FAx%80%83%F3%E6%C7%BD_%C6%5D%83%C60%D5R4gWW%97%EB%93O%3E%3Bp%F6%EC%B9%97%7Bz%AF%EF%D8P%B1%D1%BAa%C3zQ%5EV%16%0D%87%C3%BFw%E49%8F%AE_%B7%B6%03%CA%C318%F8%D5%0F%82%E3c%CFNO%CF%EC%98%8DN%14%C3%8A%3Bu*o%12q%9B%D5%12q%E6%3BF%DC%DE%C2%2F%F2%F3%7D%BFu%3A%DDm%05%05%AEpz%7BV%A95k%A9%CD%D4%DE9%3B%08%01%07%C1%0Do%F2%C8yl%0E%2B%AF%96%BBx%8A%97R%60X9%3F%91%CEQ%88%13HM%A7%F0(%81%1EW%2B%97fY%9Dr%97%26%EC%A6%E0%7B%86%60r3%1C%83%1AE3X%2F%26%D9%0FMQ%F8%E2%8B%2FNd%03%A7qA%F03%83s%95%5C%0C%9C%18O%25%F9%25%F7%F7%F9%98%9E%B4%DC%24%D0%C3L%E9%9A%8B%7C%F9%91G%1EI-8yF%40%A7D%C6%08%00%B8hK%5B%5B%9B%0E%F4%87%1F~%98%9Et5%CB%F37%16%C3%3A%E3%98%01%CA%1E%00%D8%CFw%81%40%20U%2Fx%5E%B1a%AB%B9%9D%7D%03%AF%D4%17%05%A3%0BJ%AE%8C%2C%90%22uww%FB%F8_%06%B0%7C%D5%E4%B2%9Cg%5E%F1%ACq%09r3%02%D3%7D%E2%B3%D6%03%A7%DB%DA%5E%EA%EF%EF%DB%91Lj%D6%AD%5B%B7%88%95%90I%24%12%BE%08%D4%FD%B3%DF%BF%A6%03%E3%E6%5C3%BF%8F%FA%FEofff%05%C6%B6%06%FD*%C7o%17p%13%C2%9C%0Fa%D1%5C%C3%D8%C6%20%BFD%B6%B9%B7%EE%DE%BD%DB%04%13%601%9E%5Cg'1%11M%E9%07%3Bh%A2%A8%3D%D1X%AD%D1%D3%86%E60%81G%9AU%8C%CE%E8%89K%CF_W%D5%7B%F6%EC1%A9-%3C%F5%1E%E5%3B8q%D9%3AH%E0r%B7%8A%80W%7D%A3%80%F7%ED%DB%97%F1%C4%0CI%BE%AC%DB8%E8%B8%8A%E9%E1%5D%3CSy%D4%9B*_YY%19%A7%86%E3%A4%F3%1D%DB%E3~%B8%E4%DF%BD%E9%14%06%E6%B1%EB%CA%95%2B%15%FCW%17%8E%8D%B1M%C8%24%B5%13e%00%5D%ED%DE%BD%7B%3B.%5D%BA%E4%E3%98%94%ACx5%86%91(w%D4%5D%03P%B6%5C%BDz%95Z9%15%86%82%FC4%83U%99%00%DFk%E1%A9%26%B6-%A3%01fU%07~%D7%F1%00%09Lq%EA%8C%02%17%18%F7%F5%D5.%D5%22r%9B%07%E6E%02%F3%E43%A7O%B7%BFt%E5%F2%A5%AA%C1%FE~%2B%E5%09%E7J%84%82%C1XY%D9%AA_%97%AF.%BF%08%BF%24%9E%86%17%D65%20%F3%B7N%D6%E7%9E%7B%8E%83%3D%8D%DC%9D%F6%AE1%CB%E4%D7%3F%F4%D0C%FE%0C!%90%A4%04%60P%9E%914%A6a%FE9t%E8%90%96%E1%5D%F3%22%84%BF%17%13%D5%24%CF3%1AS4Syh%24U%FF%A8%81%E7%C6%E8u%ABM%92%B4%F2%3D%B2O)%FE%B5e%CB%96%3F%E0%B2%83G%E2d%1F%F8%8E%C0%7C'%9D'H%1As%10%40%FA%99%2CO%9E%C9%B2%0DY%C6%E2%87%F7%AF%9F%E7%C4%3D%E3%C9A%D9%F6%87%E9'%C4X%DF%AE%5D%BB%EAeY%3Ec_%DB%D2%22%2Cu%C8%8D%EA%F8%1E%D2U%C3%16%F0%3Bx%1E%04hXG%40%F6%8D%B2%AC_%8A%DC%98%B0%E8%5C%9F%7D%0A%60%B6%B7%01%98%97%AB%FAz%7B%ACn%2C%A6%ED%3Bvb%F1%86c7%86o%B4A%18%C7%1F%7B%FC%B1%E0%9D%8E7%E7%FEo%FD%16%D3%A7%9F~%FAC%86%94%08%08%98T%1F%16%40%E8~%1B%E3%97_v%B9%3Eo%3DAS%FE%F2%95%2B%97w%F4%F7%F6%A6%80Y%BClYLK%24%DA6m%AA%3C%F2%D4%D3O~%E8%F7%FB%23w%BA%7Dk%0Ef%B7%96%A015E3%94%D5%B8%9F%12%9C0%F7%C9%93'%0E%B4%9D%86%C6%240%AF%1B%81Y%12%D5%92%F1%B6%CAM%1B%09%CC%96%BB%01%CC%1C8o%23%01%94%C9l%11%80%3F%F6%04N%ACs%CC%B6%F6%8E%97%BA%AE%5C%AER%C0%AC%020%8B%8AK%A2%F0.%DB*%2B76%3C%F5%D4%93%C7%EF%160s%E0%BC%8D%04%9Ei%D4%96%89%FB%09%98'O%B4%3E%D3%D6%D6%0ES~e%3B%81%E9%91%1AS%07%264%26M%F9%D3O%DF%5D%60%E6%C0y%1B%09%DEj%D4%B0u%18%BB%1F%C6t%FE%FC%25%F7g%9F%B5%1Elo%A3)%97%C0%F4z%C5%F6*%9A%F2%E2h%22%11%3F%0D%8Dy%E4%A9%A7%F6%DFu%60%E6%1C%A2%DB3%EB%EA%BF%2F%E9E%EF%FF%E3%07%E6ywk%EB%E9%83mp~%BA%BB.o%EB%BF~%7D%1E%98%BA%C6%2C%06%C7L%9E%DE%04S%FE%CC%C1%03%C7W%AF%5E%3D%FB%5D%F4)%07%CE%5C%82W%FE%A5%FB%C4%89%D6%834%E5%DD%5DWn%06f%D12%003%F1E%E5%A6%8A%86%83%07%9F%F9%E8%BB%02f%0E%9C%B9%A4%03%B3%B5%F5%D4%A1%2F%BEh%FB%C7.h%CC%01%98r%AF7_%07f!%80)%B4%C4)%9Arh%CC%EF%14%989p~%CF%D3%B9s%E7%3C%00%E5%B3%C8%2F%CD%03%F3%BA%85%87%7B%B6%EF%0Cp%A7%89%1AS%02%F3%99%EF%1C%98L%96%DC%14%7D%7F%D3%F6mU%87%CE%9C9%FBJ7%9C%9F%81%BE%5E%8B%AE1u%60%82cj%C9S%1B%C91%9F9%F0%F1%9A5kf%EFE%FFr%E0%FC%1E%A7U%AB%CA%FF%AE%BB%AB%EB%F1%C1%81%BE%3C%AF%B7%40%07f!4%A6%D0%92%9Fo%DC%B8%A1%E1%C0%81%03%9F%AC%5D%BBv%F6%5E%F5%CF%9C%9B%A2%EF18%CBV%1E%BF12%DC%EBt%B9%E3%DB%AAv%10%98sB%D3Z%2B%2B64%FC%C9%81%03%1F%DFK%60%E64%E7%F7%3C%FD%E67%FF%DD%D5%DE%D1n_%B9%AA%7C%A5%AF%B0%D0f1%9B%DA%2B%2B%2B%8E%1Cz%EE%D0'%ABV%AF%9E%BB%D7%FD%FB%7F%01%06%00%5B%B7%BC%8B%96%3B%00%06%00%00%00%00IEND%AEB%60%82"
    
function addGlobalStyle(css) {
    try {
        var elmHead, elmStyle;
        elmHead = document.getElementsByTagName('head')[0];
        elmStyle = document.createElement('style');
        elmStyle.type = 'text/css';
        elmHead.appendChild(elmStyle);
        elmStyle.innerHTML = css;
    } catch (e) {
        if (!document.styleSheets.length) {
            document.createStyleSheet();
        }
        document.styleSheets[0].cssText += css;
    }
}
    
  function drawFilledPolygon(canvas,shape)/*{{{*/
	{
		canvas.beginPath();
		canvas.moveTo(shape[0][0],shape[0][1]);

		for(p in shape)
			if (p > 0) canvas.lineTo(shape[p][0],shape[p][1]);

		canvas.lineTo(shape[0][0],shape[0][1]);
		canvas.fill();
	};
	
	function translateShape(shape,x,y)
	{
		var rv = [];
		for(p in shape)
			rv.push([ shape[p][0] + x, shape[p][1] + y ]);
		return rv;
	};
	
	function rotateShape(shape,ang)
	{
		var rv = [];
		for(p in shape)
			rv.push(rotatePoint(ang,shape[p][0],shape[p][1]));
		return rv;
	};
	
	function rotatePoint(ang,x,y)
	{
		return [
			(x * Math.cos(ang)) - (y * Math.sin(ang)),
			(x * Math.sin(ang)) + (y * Math.cos(ang))
		];
	};
	
	function drawLineArrow(canvas,x1,y1,x2,y2)
	{
		canvas.beginPath();
		canvas.moveTo(x1,y1);
		canvas.lineTo(x2,y2);
		canvas.stroke();
		var ang = Math.atan2(y2-y1,x2-x1);
		drawFilledPolygon(canvas,translateShape(rotateShape(arrow_shape,ang),x2,y2));
	};
	
	var arrow_shape = [
		[ -10, -4 ],
		[ -8, 0 ],
		[ -10, 4 ],
		[ 0, 0 ]
	];







function Node(label) {
	this.x = 0;
	this.y = 0;
	this.radius = 25;
	this.label = label;
	this.outEdges = new Array();
	this.lastShiftId = 0;
    this.labelYOffset = 0;
}

Node.prototype.draw = function(context) {
	//alert("drawing " +this.label + " at " + this.x + ", " + this.y);
	context.beginPath();
	context.arc(this.x,this.y,this.radius,0,Math.PI*2,true);
	context.closePath();
	context.stroke();
	
	context.textBaseline = 'top';
	context.fillText  (this.label, this.x-20, this.y-5+this.labelYOffset);
}

Node.prototype.shift = function(x, y, id) {
	if (this.lastShiftId != id)
	{
		this.lastShiftId = id;
		if (this.processed()) {
			this.x += x;
			this.y += y;
		}
		
		for (var i = 0; i < this.outEdges.length; i++) {
			if (this.outEdges[i] == this) continue; // to avoid problems with looping on the last node
			this.outEdges[i].shift(x, y, id);
		}
	}
}

Node.prototype.drawOutEdges = function(context) {
	for (var i = 0; i < this.outEdges.length; i++) {
		var child = this.outEdges[i];
		
		// calculate line endpoints (they should be on node circles, not centers)
		var xDiff = Math.abs(this.x - child.x);
		var yDiff = Math.abs(this.y - child.y);
		var distance = Math.sqrt(xDiff*xDiff + yDiff*yDiff);
		
		var xOffsetThis = (xDiff * this.radius) / distance;
		var yOffsetThis = (yDiff * this.radius) / distance;
		var xOffsetChild = (xDiff * child.radius) / distance;
		var yOffsetChild = (yDiff * child.radius) / distance;
		
		var thisX = this.x;
		var thisY = this.y;
		var childX = child.x;
		var childY = child.y;
		
		if (this.x > child.x) {
			thisX -= xOffsetThis;
			childX += xOffsetChild;
		} else {
			thisX += xOffsetThis;
			childX -= xOffsetChild;
		}
		if (this.y > child.y) {
			thisY -= yOffsetThis;
			childY += yOffsetChild;
		} else {
			thisY += yOffsetThis;
			childY -= yOffsetChild;
		}
		
		// Draw the line
		context.moveTo(thisX, thisY);
		context.lineTo(childX, childY);
		context.stroke();
		
		// Draw the arrowhead
		var ang = Math.atan2(childY-thisY,childX-thisX);
		drawFilledPolygon(context,translateShape(rotateShape(arrow_shape,ang),childX,childY));
	}
}

Node.prototype.processed = function() {
	return (!(this.x == 0 && this.y == 0));
}


function StartNode() {
	this.radius = 10;
}

StartNode.prototype = new Node(""); // subclass of Node

StartNode.prototype.draw = function(context) {  // override the draw method
	context.beginPath();
	context.arc(this.x,this.y,this.radius,0,Math.PI*2,true);
	context.closePath();
	context.fill();
}



function drawDAG(DAG, canvas) {
	// resize inspector (because the CSS scales it)
	canvas.width = window.innerWidth;
	canvas.height = 200
	
	//alert(DAG);
	var context=canvas.getContext("2d");
	var startX = 15;
	var startY = 30;
	var hspace = 100;
	var vspace = 75;
	var shiftId = 0;
	
	DAG = DAG.replace("%3D", "=");
	var nodeTextArray = DAG.split("//")[1].split("/");
	if (nodeTextArray[0] != "dag")
		return;
	
	// Make an array of node objects indexed by node number
	// e.g.: AD.adname=0:2,1 goes in nodeArray[0]
	var nodeArray = new Array();
	//var startingNodeIndices = new Array(); // The indices of the outgoing nodes from the implicit starting node
	var startNode;
	//nodeTextArray[0] just says "dag"
	//nodeTextArray[1] contains the out edges for the starting node
	startNode = new StartNode();
	startNode.outEdges = nodeTextArray[1].split(",");
	// the rest of the entries in nodeTextArray are regular DAG nodes
	for (var i=2; i < nodeTextArray.length; i++) {  // entry 0 just says "dag", so start at 1
		var nodeInfo = nodeTextArray[i].split("=");
		var nodeLabel = nodeInfo[0];
		var nodeNum = nodeInfo[1].split(":")[0];
		
		var node = new Node(nodeLabel);
		node.outEdges = nodeInfo[1].split(":")[1].split(",");
		nodeArray[nodeNum] = node;
	}
	
	
	// Replace indices in each node's outbound edges list with the nodes themselves
	// startNode
	for (var j = 0; j < startNode.outEdges.length; j++) {
		startNode.outEdges[j] = nodeArray[startNode.outEdges[j]];
	}
	// other nodes
	for (var i = 0; i < nodeArray.length; i++) {
		var node = nodeArray[i];
		
		for (var j = 0; j < node.outEdges.length; j++) {
			node.outEdges[j] = nodeArray[node.outEdges[j]];
		}
	}

	
	// Process & draw nodes in a BFS fashion
	// Make an empty queue
	var nodesToDraw = new Array();
	
	// Initialize the queue with the starting node
	startNode.x = startX;
	startNode.y = startY;
	nodesToDraw.push(startNode);
	
	
	// Now set each node's position
	while (nodesToDraw.length > 0) {
		var node = nodesToDraw.shift();
		var nextY = node.y;
		
		// process children
		for (var i = 0; i < node.outEdges.length; i++) {
			var child = node.outEdges[i];
			if (child == node) continue; // to avoid problems with looping on the last node
			
			if (!child.processed())
			{
				child.x = node.x + hspace;
				child.y = nextY;
                child.labelYOffset = 10*((child.x-startNode.x)/hspace - 1); // try to stagger labels vertically based on how many nodes are already in this row
				nextY += vspace;
				nodesToDraw.push(child);
			} 
			else if (child.x <= node.x) 
			{
				child.shift((node.x - child.x + hspace), 0, shiftId);
				shiftId++;
				nextY = child.y + vspace;
			}
		}
	}
	
	
	// Now that the positions have been set, draw the nodes and their edges
	startNode.draw(context);
	startNode.drawOutEdges(context);
	for (var i = 0; i < nodeArray.length; i++) {
		var node = nodeArray[i];
		node.draw(context);
		node.drawOutEdges(context);
	}
}

function mouseover(e) {
	var inspector = document.getElementById("xia-inspector-canvas");
	
	var sender = (e && e.target) || (window.event && window.event.srcElement);
	drawDAG(sender.href, inspector);
}

function mouseout(e) {
	var inspector = document.getElementById("xia-inspector-canvas");
	var context = inspector.getContext("2d");
	
	// Store the current transformation matrix
	context.save();
	
	// Use the identity matrix while clearing the canvas
	context.setTransform(1, 0, 0, 1, 0, 0);
	context.clearRect(0, 0, inspector.width, inspector.height);
	
	// Restore the transform
	context.restore();
}

function toggleInspector() {
	var inspector = document.getElementById("xia-inspector");
	var placeholder = document.getElementById("xia-inspector-placeholder");
	if (inspector.style.display == "none") {
		inspector.style.display = "";
		placeholder.style.display = "";
	} else {
		inspector.style.display = "none";
		placeholder.style.display = "none";
	}
}


//===== "MAIN" =====//

// Add some CSS
addGlobalStyle(css);
/*
CSS Browser Selector v0.4.0 (Nov 02, 2010)
Rafael Lima (http://rafael.adm.br)
http://rafael.adm.br/css_browser_selector
License: http://creativecommons.org/licenses/by/2.5/
Contributors: http://rafael.adm.br/css_browser_selector#contributors
*/
function css_browser_selector(u){var ua=u.toLowerCase(),is=function(t){return ua.indexOf(t)>-1},g='gecko',w='webkit',s='safari',o='opera',m='mobile',h=document.documentElement,b=[(!(/opera|webtv/i.test(ua))&&/msie\s(\d)/.test(ua))?('ie ie'+RegExp.$1):is('firefox/2')?g+' ff2':is('firefox/3.5')?g+' ff3 ff3_5':is('firefox/3.6')?g+' ff3 ff3_6':is('firefox/3')?g+' ff3':is('gecko/')?g:is('opera')?o+(/version\/(\d+)/.test(ua)?' '+o+RegExp.$1:(/opera(\s|\/)(\d+)/.test(ua)?' '+o+RegExp.$2:'')):is('konqueror')?'konqueror':is('blackberry')?m+' blackberry':is('android')?m+' android':is('chrome')?w+' chrome':is('iron')?w+' iron':is('applewebkit/')?w+' '+s+(/version\/(\d+)/.test(ua)?' '+s+RegExp.$1:''):is('mozilla/')?g:'',is('j2me')?m+' j2me':is('iphone')?m+' iphone':is('ipod')?m+' ipod':is('ipad')?m+' ipad':is('mac')?'mac':is('darwin')?'mac':is('webtv')?'webtv':is('win')?'win'+(is('windows nt 6.0')?' vista':''):is('freebsd')?'freebsd':(is('x11')||is('linux'))?'linux':'','js']; c = b.join(' '); h.className += ' '+c; return c;}; css_browser_selector(navigator.userAgent);


// Add the inspector
var button_div = document.createElement("div");
button_div.className = "top-right-button";
document.body.appendChild(button_div);

var button_link = document.createElement("a");
button_link.href = "#";
button_link.innerHTML = "<img src=\"" + inspector_image + "\" />";
button_link.addEventListener("click", toggleInspector, true);
button_div.appendChild(button_link);

var inspector = document.createElement("div");
inspector.id = "xia-inspector";
inspector.className = "persistent-footer inspector";
inspector.style.display = "none";
inspector.innerHTML = "<p style=\"margin:0; font-family: sans-serif; font-size:14pt; font-style:bold\"><img src=\"" + inspector_title_image + "\" /></p>";
document.body.appendChild(inspector);

var canvas_div = document.createElement("div");
canvas_div.id = "canvas-div";
canvas_div.innerHTML = "<canvas style=\"height: 200px\" id=\"xia-inspector-canvas\"></canvas>";
inspector.appendChild(canvas_div);

var placeholder = document.createElement("div");
placeholder.id = "xia-inspector-placeholder";
placeholder.style.height = "200px";
placeholder.style.display = "none";
document.body.appendChild(placeholder);

// Now modify all XIA links
var allLinks = document.links;
for (var i=0; i<allLinks.length; i++) {
	if (allLinks[i].href.indexOf("dag") != -1) // TODO: better test for an XIA address -- regex?
	{
        allLinks[i].className = "xia-link";
		allLinks[i].onmouseover = mouseover;
		allLinks[i].onmouseout = mouseout;
	}
}