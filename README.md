Çalışma kapsamında aşağıdaki özelliklere sahip bir Mini Filter Driver geliştirilmiştir:
- User tarafından belirlenen bir dizindeki dosyaların silme işlemlerini tespit eder,
- User'a silme işlemine ait bilgileri içeren bir log mesajı gönderir, mesaj içerisinde aşağıdaki bilgiler yer almaktadır:
  - Tarih,
  - İşlem türü,
  - Silinecek dosyanın path'i,
  - Silme işlemini yapan process'in adı.
- Ve dosyanın silinmesi engeler.


Gerekli çalışma ortamının kurulumu için aşağıdaki link takip edilebilir.
"https://learn.microsoft.com/en-us/windows-hardware/drivers/ifs/development-and-testing-tools"


Mini Filter Driver'ın Yüklenmesi için gereken adımlar:
- Proje clone'lanır ve isual Studio kullanılarak build edilir.
- Build sonucu oluşan driver dosyaları (monitorfiledeletion.cat, monitorfiledeletion.inf, monitorfiledeletion.sys) "C:\Windows\System32\drivers" dizinine kopyalanır.
- "monitorfiledeletion.inf" dosyası install edilir.(Dosya üzerine sağ tık yapılarak install seçeneğine ulaşılabilir.)
- Command Prompt(cmd) Administrator modunda çalıştırılarak "C:\Windows\System32\drivers" dizinine gidilir.
- "fltmc load monitorfiledeletion" komutu çalıştırılarak mini-filter driver load edilir.
- User uygulaması(monitorfiledeletion.exe) çalıştırılır.
  ! Bu aşamada "Could not connect to filter: 0x80070005" hatası alınmaktadır.
  Hatanın çözümü için araştırmalar yapılmış, bulunan çözümler denenmiş fakat hata çözülememiştir.

Yazılan driver'ın fonksiyonel testlerinde, yetki sınırlamasıyla file system'de dosya silme işlemini engellediği gözlemlenmiştir.
Mini-filter driver ve user agent'ta haberleşme kanalı oluturulmuş ancak permission hatası sebebiyle bağlantı sağlanamamış, 
bu sebeple program uçtan uca test edilememiştir.
  
