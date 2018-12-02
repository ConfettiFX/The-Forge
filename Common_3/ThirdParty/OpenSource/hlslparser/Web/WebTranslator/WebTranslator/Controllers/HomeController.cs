using System;
using System.IO;
using System.Net;
using System.Web;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Threading.Tasks;
using Microsoft.AspNetCore.Mvc;
using Microsoft.AspNetCore.Hosting;

using WebTranslator.Models;


using System.Net.Mail;

using Parser;

namespace WebTranslator.Controllers
{
    public class HomeController : Controller
    {
        public DataPackage dataPackage;

        static public string GLanguage;
        static public string GShader;

        /*
        public IActionResult Index()
        {
            return View();
        }
        */

        private IHostingEnvironment _env;
        public HomeController(IHostingEnvironment env)
        {
            _env = env;
        }

        public IActionResult About()
        {
            ViewData["Message"] = "Your application description page.";

            return View();
        }

        public IActionResult Contact()
        {
            ViewData["Message"] = "Your contact page.";

            return View();
        }

        public IActionResult Error()
        {
            return View(new ErrorViewModel { RequestId = Activity.Current?.Id ?? HttpContext.TraceIdentifier });
        }

        public ActionResult PassData()
        {
            return View();
        }


        public ActionResult SetandLanguage(string Language, string Shader, string Entry, string FileName, string FileContents)
        {
            GLanguage = Language;
            return View();
        }

        public ActionResult SetShader(string Shader)
        {
            GShader = Shader;
            return View();
        }

        public ActionResult Index(DataPackage package)
        {
            return View();
        }

        public void CreateIncludeFile(DataPackage package)
        {
            if (package.FileName != null)
            {
                ParserCS.CreateInlcudeFile(package.FileContents, Path.GetFileNameWithoutExtension(package.FileName), Path.GetExtension(package.FileName));
            }
            else
            {
                return;
            }
            
        }

        [HttpPost]
        public IActionResult Parse([FromBody] DataPackage package)
        {
            if (package.FileName != null)
            {
                var result =  ParserCS.Parsing(package.FileName, package.FileContents, package.EntryName == null ? "main" : package.EntryName, package.Shader, package.Language);

                var data = new DataPackage() { Result = result };

                return Json(data);
            }
            else
            {
                return Json(new DataPackage());
            }
        }

        public DataPackage LocalParse(DataPackage package)
        {
            if (package.FileName != null)
            {
                var result = ParserCS.Parsing(package.FileName, package.FileContents, package.EntryName == null ? "main" : package.EntryName, package.Shader, package.Language);

                var data = new DataPackage() { Result = result };

                return data;
            }
            else
            {
                return null;
            }
        }


        public DataPackage GetStoredResult(DataPackage package)
        {
            var result = ParserCS.GetStoredResult(package.Language);

            var data = new DataPackage() { Result = result };

            return data;
        }


        


        public void SaveFeedbackFromClient(DataPackage dataPackage)
        {
            var webRoot = _env.WebRootPath;

            var fromAddress = "confetti.shader.translator@gmail.com";
            var toAddress = "confetti.shader.translator@gmail.com";

            MailMessage mail = new MailMessage(toAddress, fromAddress);
            SmtpClient client = new SmtpClient();
            client.Port = 587;
            client.DeliveryMethod = SmtpDeliveryMethod.Network;
            client.UseDefaultCredentials = false;
            client.EnableSsl = true;
            client.Host = "smtp.gmail.com";
            client.Credentials = new NetworkCredential(fromAddress, "Schwarzenegger3K@");


            mail.Subject = "translation error report : ";
            mail.Body = dataPackage.FileContents;
            client.Send(mail);
        }


        [HttpPost]
        public IActionResult SelectShader([FromBody] DataPackage model)
        {
            return Json(new DataPackage());
        }
    }
}
