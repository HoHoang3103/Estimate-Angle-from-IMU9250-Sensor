using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using ZedGraph;
using System.IO;
using System.IO.Ports;
namespace WindowsFormsApp1
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();
            string[] Baudrate = { "1200", "2400", "4800", "9600", "115200" };
            cboBaudRate.Items.AddRange(Baudrate);
            Control.CheckForIllegalCrossThreadCalls = false;
        }

        private void Form1_Load(object sender, EventArgs e)
        {
            cboComPort.DataSource = SerialPort.GetPortNames(); //detect automatically
            cboBaudRate.Text = "9600"; //default
            GraphPane myPanne = zedGraphControl1.GraphPane; //khoi tao bieu do 1
            GraphPane myPanne2 = zedGraphControl2.GraphPane; //khoi tao bieu do 2
            GraphPane myPanne3 = zedGraphControl3.GraphPane; //khoi tao bieu do 3
            myPanne.Title.Text = "Plotted graph of roll";
            myPanne.YAxis.Title.Text = "Roll angle (degree)";
            myPanne.XAxis.Title.Text = "Time (s)";
            myPanne2.Title.Text = "Plotted graph of pitch";
            myPanne2.YAxis.Title.Text = "Pitch angle (degree)";
            myPanne2.XAxis.Title.Text = "Time (s)";
            myPanne3.Title.Text = "Plotted graph of yaw";
            myPanne3.YAxis.Title.Text = "Yaw angle (degree)";
            myPanne3.XAxis.Title.Text = "Time (s)";
            RollingPointPairList list = new RollingPointPairList(500000); // 500000 points
            RollingPointPairList list2 = new RollingPointPairList(500000);
            RollingPointPairList list3 = new RollingPointPairList(500000);
            LineItem line = myPanne.AddCurve("angle", list, Color.Red, SymbolType.Diamond); //chu thich
            LineItem line2 = myPanne2.AddCurve("angle", list2, Color.Blue, SymbolType.Square);
            LineItem line3 = myPanne3.AddCurve("angle", list3, Color.Green, SymbolType.Circle);

            myPanne.XAxis.Scale.Min = 0;
            myPanne.XAxis.Scale.Max = 40;
            myPanne.XAxis.Scale.MinorStep = 0.05; // vach chia nho
            myPanne.XAxis.Scale.MajorStep = 0.1; // vach chia to x

            myPanne.YAxis.Scale.Min = -255;
            myPanne.YAxis.Scale.Max = 255;
            myPanne.YAxis.Scale.MinorStep = 10; // vach chia nho
            myPanne.YAxis.Scale.MajorStep = 20; // vach chia to y

            zedGraphControl1.AxisChange();

            myPanne2.XAxis.Scale.Min = 0;
            myPanne2.XAxis.Scale.Max = 40;
            myPanne2.XAxis.Scale.MinorStep = 0.05; // vach chia nho
            myPanne2.XAxis.Scale.MajorStep = 0.1; // vach chia to x

            myPanne2.YAxis.Scale.Min = -255;
            myPanne2.YAxis.Scale.Max = 255;
            myPanne2.YAxis.Scale.MinorStep = 10; // vach chia nho
            myPanne2.YAxis.Scale.MajorStep = 20; // vach chia to y

            zedGraphControl2.AxisChange();

            myPanne3.XAxis.Scale.Min = 0;
            myPanne3.XAxis.Scale.Max = 40;
            myPanne3.XAxis.Scale.MinorStep = 0.05; // vach chia nho
            myPanne3.XAxis.Scale.MajorStep = 0.1; // vach chia to x

            myPanne3.YAxis.Scale.Min = -255;
            myPanne3.YAxis.Scale.Max = 255;
            myPanne3.YAxis.Scale.MinorStep = 10; // vach chia nho
            myPanne3.YAxis.Scale.MajorStep = 20; // vach chia to y

            zedGraphControl3.AxisChange();
        }

        private void label1_Click(object sender, EventArgs e)
        {

        }

        private void label3_Click(object sender, EventArgs e)
        {

        }

        private void butConnect_Click(object sender, EventArgs e)
        {
            if (!serCOM.IsOpen)
            {
                butConnect.Text = "Disconnected";
                serCOM.PortName = cboComPort.Text;
                serCOM.BaudRate = Convert.ToInt32(cboBaudRate.Text);
                serCOM.Open();
            }
            else
            {
                butConnect.Text = "Connected";
                serCOM.Close();
            }
        }

        private void butExit_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        private void serCOM_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            string revdata = serCOM.ReadExisting(); // ko dung ham .ReadLine(); duoc
            txtAllData.Text = revdata;

            revdata = revdata.Trim(); //gsu @10AT27.34#, 0XXXXX 0XXXXX 0XXXXX
            int len = revdata.Length; //len = 20
            if (revdata[0] == '-')
            // [1]=='1', [2]== '0', [3]== 'A' dung ham IndexOf de tim ra vi tri cua 1 ky tu
            // int vt0 = revdata.IndexOf('0'); //->vt=0
            {
                string roll = revdata.Substring(0, 7); //lay sau vt0 (1->5 so)
                double gtA = double.Parse(roll);
                double gtRoll = gtA / 1000;
                string rollAngle = gtRoll.ToString();
                txtRoll.Text = rollAngle;
                Invoke(new MethodInvoker(() => draw(Convert.ToDouble(rollAngle))));
                if (revdata[8] == '-')
                {
                    string pitch = revdata.Substring(8, 7); //lay sau T (1->5 so)
                    double gtB = double.Parse(pitch);
                    double gtPitch = gtB / 1000;
                    string pitchAngle = gtPitch.ToString();
                    txtPitch.Text = pitchAngle;
                    Invoke(new MethodInvoker(() => draw2(Convert.ToDouble(pitchAngle))));
                    if (revdata[17] == '-')
                    {
                        string yaw = revdata.Substring(17, 7); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                    else
                    {
                        string yaw = revdata.Substring(17, 6); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                }
                else
                {
                    string pitch = revdata.Substring(8, 6); //lay sau T (1->5 so)
                    double gtB = double.Parse(pitch);
                    double gtPitch = gtB / 1000;
                    string pitchAngle = gtPitch.ToString();
                    txtPitch.Text = pitchAngle;
                    Invoke(new MethodInvoker(() => draw2(Convert.ToDouble(pitchAngle))));
                    if (revdata[16] == '-')
                    {
                        string yaw = revdata.Substring(16, 7); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                    else
                    {
                        string yaw = revdata.Substring(16, 6); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                }
            }
            else
            {
                string roll = revdata.Substring(0, 6); //lay sau vt0 (1->5 so)
                double gtA = double.Parse(roll);
                double gtRoll = gtA / 1000;
                string rollAngle = gtRoll.ToString();
                txtRoll.Text = rollAngle;
                Invoke(new MethodInvoker(() => draw(Convert.ToDouble(rollAngle)))); // in ra do thi }



                if (revdata[7] == '-')
                {
                    string pitch = revdata.Substring(7, 7); //lay sau T (1->5 so)
                    double gtB = double.Parse(pitch);
                    double gtPitch = gtB / 1000;
                    string pitchAngle = gtPitch.ToString();
                    txtPitch.Text = pitchAngle;
                    Invoke(new MethodInvoker(() => draw2(Convert.ToDouble(pitchAngle))));
                    if (revdata[16] == '-')
                    {
                        string yaw = revdata.Substring(16, 7); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                    else
                    {
                        string yaw = revdata.Substring(16, 6); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                }
                else
                {
                    string pitch = revdata.Substring(7, 6); //lay sau T (1->5 so)
                    double gtB = double.Parse(pitch);
                    double gtPitch = gtB / 1000;
                    string pitchAngle = gtPitch.ToString();
                    txtPitch.Text = pitchAngle;
                    Invoke(new MethodInvoker(() => draw2(Convert.ToDouble(pitchAngle))));
                    if (revdata[15] == '-')
                    {
                        string yaw = revdata.Substring(15, 7); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                    else
                    {
                        string yaw = revdata.Substring(15, 6); //lay sau T (1->5 so)
                        double gtC = double.Parse(yaw);
                        double gtYaw = gtC / 1000;
                        string yawAngle = gtYaw.ToString();
                        txtYaw.Text = yawAngle;
                        Invoke(new MethodInvoker(() => draw3(Convert.ToDouble(yawAngle))));
                    }
                }



            }
        }
        double tong1 = 0;
        double tong2 = 0;
        double tong3 = 0;
        public void draw(double line)
        {
            LineItem duongline = zedGraphControl1.GraphPane.CurveList[0] as LineItem;
            if (duongline == null)
            {
                return;
            }
            IPointListEdit list = duongline.Points as IPointListEdit;
            if (list == null)
            {
                return;
            }

            list.Add(tong1, line);
            zedGraphControl1.AxisChange();
            zedGraphControl1.Invalidate();
            tong1 += 0.1; //1 step x = vach chia to
        }

        public void draw2(double line2)
        {
            LineItem duongline2 = zedGraphControl2.GraphPane.CurveList[0] as LineItem;
            if (duongline2 == null)
            {
                return;
            }
            IPointListEdit list = duongline2.Points as IPointListEdit;
            if (list == null)
            {
                return;
            }

            list.Add(tong2, line2);
            zedGraphControl2.AxisChange();
            zedGraphControl2.Invalidate();
            tong2 +=0.1; //1 step x = vach chia to
        }

        public void draw3(double line3)
        {
            LineItem duongline3 = zedGraphControl3.GraphPane.CurveList[0] as LineItem;
            if (duongline3 == null)
            {
                return;
            }
            IPointListEdit list = duongline3.Points as IPointListEdit;
            if (list == null)
            {
                return;
            }

            list.Add(tong3, line3);
            zedGraphControl3.AxisChange();
            zedGraphControl3.Invalidate();
            tong3 +=0.1; //1 step x = vach chia to x
        }

        private void butSaveData_Click(object sender, EventArgs e)
        {
            TextWriter text1 = new StreamWriter("G:\\Data.txt");
            text1.WriteLine(txtAllData);
            text1.Close();
        }
    }
}
