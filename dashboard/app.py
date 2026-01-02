import streamlit as st
from supabase import create_client, Client
from streamlit_option_menu import option_menu
import pandas as pd
import time
from st_aggrid import AgGrid, GridOptionsBuilder, DataReturnMode, AgGridTheme
from streamlit_autorefresh import st_autorefresh
import base64



# Inisialisasi Supabase
SUPABASE_URL = "YOUR_SUPABASE_URL"
SUPABASE_KEY = "YOUR_SUPABASE_KEY"

supabase: Client = create_client(SUPABASE_URL, SUPABASE_KEY)

logo_wide = "images/logo_jagaguppy_wide.png"
logo_icon = "images/logo_jagaguppy_center 2.png"

def get_base64_image(image_path):
    with open(image_path, "rb") as img_file:
        return base64.b64encode(img_file.read()).decode()

def get_perangkat_list():
    response = supabase.table("perangkat").select("id").execute()
    return list({row["id"] for row in response.data})

def get_usertank_by_user():
      if 'user_id' in st.session_state:
          user_id = st.session_state.user_id
          response = supabase.table('usertank').select('*').eq('user_id', user_id).execute()
          return response.data
      else:
          st.warning("Please login first.")
          return []

def get_users():
    return supabase.table('users').select('id', 'nama').execute().data

def get_perangkat():
    return supabase.table('perangkat').select('id', 'nama').execute().data

def get_tank_types():
    return supabase.table('tank_type').select('id', 'type_tank').execute().data

def get_guppy_types():
    return supabase.table('guppy_type').select('id', 'jenis').execute().data

def apply_custom_css():
    custom_css = """
    <style>
        /* Sembunyikan elemen default Streamlit */
        #MainMenu, header,
        div[data-testid="stDecoration"],
        div[data-testid="stStatusWidget"],
        div.embeddedAppMetaInfoBar_container__DxxL1 {
            visibility: hidden;
            height: 0;
            position: fixed;
        }

        /* Tampilan tombol */
        .stButton>button {
            width: 100%;
            height: 50px;
            font-size: 20px;
        }

        .stSpinner {
            visibility: hidden;
        }

        /* Footer transparan */
        .footer {
            position: fixed;
            left: 0;
            bottom: 0;
            width: 100%;
            background-color: rgba(240, 242, 246, 0.2); /* Warna light dengan semi-transparan */
            color: #31333F; /* Warna teks gelap agar terbaca di background terang */
            text-align: center;
            padding: 10px 0;
            font-size: 14px;
            border-top: 1px solid #E6EAF1; /* Border terang */
            z-index: 100;
            backdrop-filter: blur(6px);  /* efek blur belakang */
        }
        /* Atur ukuran sidebar tetap (misal: 300px) dan tidak bisa dipersempit */
        [data-testid="stSidebar"] {
            min-width: 300px !important;
            max-width: 300px !important;
            width: 300px !important;
            overflow: auto;  /* scroll jika konten panjang */
        }

        /* Hilangkan tombol collapse sidebar (ikon ◀️) */
        [data-testid="collapsedControl"] {
            display: none !important;
        }
    </style>

    <div class="footer">
        © 2025 Sistem Monitoring Guppy — Muhammad Ilham Gymnastiar
    </div>
    """
    st.markdown(custom_css, unsafe_allow_html=True)



def table(df, status):
    # Konversi status checkbox menjadi boolean
    if status == 'aktif':
        status = True
    elif status == 'tidak':
        status = False
    else:
        status = False
    gb = GridOptionsBuilder.from_dataframe(df)
    gb.configure_default_column(
        editable=False, filter=False, resizable=True, sortable=True, value=True,
        enablePivot=True, enableValue=True, floatingFilter=True, aggFunc='sum',
        flex=1, minWidth=150, width=150, maxWidth=200
    )
    gb.configure_selection(selection_mode='multiple', use_checkbox=status)
    gb.configure_pagination(enabled=True, paginationAutoPageSize=True)
    gb.configure_grid_options(quickFilter=True)
    gridOptions = gb.build()
    custom_css = {
        ".ag-root-wrapper": {"background-color": "rgba(255,255,255,0.0)"},
        ".ag-header": {"background-color": "rgba(255,255,255,0.0)"},
        ".ag-row": {"background-color": "rgba(255,255,255,0.0)"},
        "#gridToolBar": {"padding-bottom": "0px !important"},
    }

    # Konfigurasi Grid - Mengubah tema AgGrid ke ALPINE yang lebih terang
    grid = AgGrid(
        df,
        gridOptions=gridOptions,
        data_return_mode=DataReturnMode.AS_INPUT,
        update_on='MANUAL',
        enable_quicksearch=True,
        fit_columns_on_grid_load=True,
        theme=AgGridTheme.ALPINE, # Mengubah tema AgGrid menjadi terang
        enable_enterprise_modules=True,
        height=600,
        width='100%',
        custom_css=custom_css

    )

    return grid

# Fungsi Login
def login():
    st.markdown("""
        <style>
        .center-logo {
            display: flex;
            justify-content: center;
            align-items: center;
            margin-top: -15vh;
    
            margin-bottom: 1rem;
        }
        body {
            overflow-x: hidden;      /* Hindari scroll horizontal */
        }
        </style>
    """, unsafe_allow_html=True)

    # Logo di tengah
    st.markdown(f"""
        <div class="center-logo">
            <img src="data:image/png;base64,{get_base64_image('images/logo_jagaguppy_center 2.png')}" width="270"/>
        </div>
    """, unsafe_allow_html=True)

    st.subheader("🔐 Login")
    with st.form("login_form"):
        email = st.text_input("Email", key="login_email")
        password = st.text_input("Password", type="password", key="login_password")
        submit = st.form_submit_button("Masuk")

    if submit:
        if email and password:
            result = supabase.table("users").select("*").eq("email", email).eq("password", password).execute()
            if result.data:
                user = result.data[0]
                st.session_state.logged_in = True
                st.session_state.user = user
                st.success(f"✅ Selamat datang, {user['nama']}!")
                st.session_state.selected_page = 'Dashboard'
                st.rerun()
            else:
                st.error("❌ Email atau password salah.")
        else:
            st.warning("Mohon isi semua kolom.")


def peringatan_bahaya(data_sensor):
    data_sensor = data_sensor.iloc[-1]

    ph = data_sensor['ph']
    suhu = data_sensor['suhu']
    tds = data_sensor['tds']

    if ph > 7.5:
        st.toast("⚠️ pH terlalu tinggi (basa) → Aktifkan pompa pH Down", icon="⚠️")
    elif ph < 6.5:
        st.toast("⚠️ pH terlalu rendah (asam) → Aktifkan pompa pH Up", icon="⚠️")

    if suhu < 24.0:
        st.toast("❄️ Suhu terlalu dingin → Heater menyala", icon="❄️")
    elif suhu > 28.0:
        st.toast("🔥 Suhu terlalu panas → Peltier menyala", icon="🔥")

    if tds < 400 or tds > 600:
        st.toast("💧 TDS di luar rentang optimal → Perlu pergantian air", icon="💧")


def admin_dashboard():
    # Auto-refresh setiap 5 detik
    st_autorefresh(interval=5000, limit=None, key="sensor_update")

    def ambil_data_sensor(perangkat_id):
        response = supabase.table("sensor_data") \
            .select("*") \
            .eq("perangkat_id", perangkat_id) \
            .order("created_at", desc=True) \
            .limit(100).execute()

        df = pd.DataFrame(response.data)
        if not df.empty:
            df["created_at"] = pd.to_datetime(df["created_at"])
            df = df.sort_values("created_at")
        return df

    def ambil_data_aktuator(perangkat_id):
        response = supabase.table("aktuator_data") \
            .select("*") \
            .eq("perangkat_id", perangkat_id) \
            .order("created_at", desc=True) \
            .limit(1).execute()

        df = pd.DataFrame(response.data)
        if not df.empty:
            df["created_at"] = pd.to_datetime(df["created_at"])
        return df

    perangkat_list = get_perangkat_list()
    selected_perangkat = st.sidebar.selectbox("🎛️ Pilih Perangkat", perangkat_list)

    df_sensor = ambil_data_sensor(selected_perangkat)
    df_aktuator = ambil_data_aktuator(selected_perangkat)

    if df_sensor.empty or df_aktuator.empty:
        st.warning("Belum ada data sensor atau aktuator.")
        return

    latest_sensor = df_sensor.iloc[-1]
    latest_aktuator = df_aktuator.iloc[-1]

    peringatan_bahaya(df_sensor)
    st.subheader("📊 Data Terkini")
    col1, col2, col3 = st.columns(3)
    col1.metric("🌡️ Suhu (°C)", f"{latest_sensor['suhu']:.1f}")
    col2.metric("🧪 pH", f"{latest_sensor['ph']:.2f}")
    col3.metric("💧 TDS (ppm)", f"{latest_sensor['tds']:.0f}")

    st.subheader("🛠️ Status Aktuator")
    col1, col2 = st.columns(2)

    col1.write(f"🚰 Pompa pH Up: {'🟢 ON' if latest_aktuator['pumpphup'] else '⚪ OFF'}")
    col2.write(f"🚰 Pompa pH Down: {'🟢 ON' if latest_aktuator['pumpphdown'] else '⚪ OFF'}")
    col1.write(f"🔥 Heater: {'🟢 ON' if latest_aktuator['heater'] else '⚪ OFF'}")
    col2.write(f"❄️ Peltier: {'🟢 ON' if latest_aktuator['peltier'] else '⚪ OFF'}")

    st.subheader("🌡️ Grafik Suhu (°C)")
    st.line_chart(df_sensor.set_index("created_at")[["suhu"]])

    st.subheader("🧪 Grafik pH")
    st.line_chart(df_sensor.set_index("created_at")[["ph"]])

    st.subheader("💧 Grafik TDS (ppm)")
    st.line_chart(df_sensor.set_index("created_at")[["tds"]])



def user_dashboard():
    # Auto-refresh setiap 5 detik
    st_autorefresh(interval=5000, limit=None, key="sensor_update")

    def ambil_data_sensor(perangkat_id):
        response = supabase.table("sensor_data") \
            .select("*") \
            .eq("perangkat_id", perangkat_id) \
            .order("created_at", desc=True) \
            .limit(100).execute()

        df = pd.DataFrame(response.data)
        if not df.empty:
            df["created_at"] = pd.to_datetime(df["created_at"])
            df = df.sort_values("created_at")
        return df

    def ambil_data_aktuator(perangkat_id):
        response = supabase.table("aktuator_data") \
            .select("*") \
            .eq("perangkat_id", perangkat_id) \
            .order("created_at", desc=True) \
            .limit(1).execute()

        df = pd.DataFrame(response.data)
        if not df.empty:
            df["created_at"] = pd.to_datetime(df["created_at"])
        return df

    perangkat_list = get_perangkat_list()
    selected_perangkat = st.sidebar.selectbox("🎛️ Pilih Perangkat", perangkat_list)

    df_sensor = ambil_data_sensor(selected_perangkat)
    df_aktuator = ambil_data_aktuator(selected_perangkat)

    if df_sensor.empty or df_aktuator.empty:
        st.warning("Belum ada data sensor atau aktuator.")
        return

    latest_sensor = df_sensor.iloc[-1]
    latest_aktuator = df_aktuator.iloc[-1]

    peringatan_bahaya(df_sensor)
    st.subheader("📊 Data Terkini")
    col1, col2, col3 = st.columns(3)
    col1.metric("🌡️ Suhu (°C)", f"{latest_sensor['suhu']:.1f}")
    col2.metric("🧪 pH", f"{latest_sensor['ph']:.2f}")
    col3.metric("💧 TDS (ppm)", f"{latest_sensor['tds']:.0f}")

    col1, col2 = st.columns(2)

    col1.write(f"🚰 Pompa pH Up: {'🟢 ON' if latest_aktuator['pumpphup'] else '⚪ OFF'}")
    col2.write(f"🚰 Pompa pH Down: {'🟢 ON' if latest_aktuator['pumpphdown'] else '⚪ OFF'}")
    col1.write(f"🔥 Heater: {'🟢 ON' if latest_aktuator['heater'] else '⚪ OFF'}")
    col2.write(f"❄️ Peltier: {'🟢 ON' if latest_aktuator['peltier'] else '⚪ OFF'}")

    st.subheader("🌡️ Grafik Suhu (°C)")
    st.line_chart(df_sensor.set_index("created_at")[["suhu"]])

    st.subheader("🧪 Grafik pH")
    st.line_chart(df_sensor.set_index("created_at")[["ph"]])

    st.subheader("💧 Grafik TDS (ppm)")
    st.line_chart(df_sensor.set_index("created_at")[["tds"]])


def atur_jadwal_pakan():
    user = st.session_state.user
    def get_usertank_by_user():
        response = supabase.table('usertank').select('*').eq('user_id', user['id']).execute()
        return response.data

    st.subheader('🔄 Atur Jadwal Pakan')

    perangkat_list = get_perangkat_list()
    selected_perangkat_id = st.sidebar.selectbox("🎛️ Pilih Perangkat", perangkat_list)

    # Fetch data
    users = get_users()
    perangkat = get_perangkat()
    tank_types = get_tank_types()

    # Convert to dictionaries
    user_options = {user['id']: user['nama'] for user in users}
    perangkat_options = {p['id']: p['nama'] for p in perangkat}
    tank_type_options = {tank['id']: tank['type_tank'] for tank in tank_types}

    # Fetch usertank by perangkat_id
    response = supabase.table('usertank').select('*').eq('perangkat_id', selected_perangkat_id).execute()
    data = response.data

    if not data:
        st.warning(f"Tidak ada data UserTank untuk perangkat_id: {selected_perangkat_id}")
        return

    usertank_data = data[0]

    selected_user_id = usertank_data['user_id']
    selected_tank_type_id = usertank_data['tank_id']

    # Dropdowns with default selections
    st.text_input("ID UserTank", usertank_data["id"], disabled=True)
    st.text_input("User ID", user["nama"], disabled=True)
    st.selectbox("📟 Pilih Perangkat", list(perangkat_options.values()),
                 index=list(perangkat_options.keys()).index(selected_perangkat_id), disabled=True)
    selected_tank_type = st.selectbox("🐟 Pilih Tipe Akuarium", list(tank_type_options.values()),
                                      index=list(tank_type_options.keys()).index(selected_tank_type_id))

    # Jika ingin menyimpan perubahan tipe tank:
    if st.button("💾 Simpan Perubahan"):
        updated_tank_id = [tid for tid, name in tank_type_options.items() if name == selected_tank_type][0]
        supabase.table('usertank').update({'tank_id': updated_tank_id,'user_id':user['id']}).eq('id', usertank_data["id"]).execute()
        st.success("Jadwal pakan berhasil diperbarui berdasarkan tipe akuarium.")
        st.toast("💾 Perubahan Jadwal Pakan Tersimpan", icon="💾")

def riwayat_sensor():
    st.subheader("📊 Riwayat Pembacaan Sensor Kualitas Air")
    perangkat_list = get_perangkat_list()
    perangkat_id = st.selectbox("🔧 Pilih Perangkat", perangkat_list)

    # Ambil data dari Supabase
    data = (
        supabase.table("sensor_data")
        .select("*")
        .eq("perangkat_id", perangkat_id)
        .order("created_at", desc=True)
        .limit(200)
        .execute()
        .data
    )

    df = pd.DataFrame(data)

    if not df.empty:
        df["created_at"] = pd.to_datetime(df["created_at"], format='ISO8601')
        df = df.sort_values("created_at")

        # Statistik ringkas
        st.subheader("📌 Ringkasan Statistik")
        col1, col2, col3 = st.columns(3)
        col1.metric("Rata-rata Suhu", f"{df['suhu'].mean():.2f} °C")
        col2.metric("Rata-rata pH", f"{df['ph'].mean():.2f}")
        col3.metric("Rata-rata TDS", f"{df['tds'].mean():.0f} ppm")

        # Visualisasi
        st.subheader("📈 Visualisasi Data Sensor")

        st.markdown("#### Grafik Suhu (°C)")
        st.line_chart(df.set_index("created_at")[["suhu"]])

        st.markdown("#### Grafik pH")
        st.line_chart(df.set_index("created_at")[["ph"]])

        st.markdown("#### Grafik TDS (ppm)")
        st.line_chart(df.set_index("created_at")[["tds"]])

        # Tabel data
        st.subheader("📋 Tabel Data Sensor")
        st.dataframe(df[::-1], use_container_width=True)

    else:
        st.warning("⚠️ Tidak ditemukan data sensor untuk perangkat yang dipilih.")

def riwayat_aktuator():
    st.subheader("⚙️ Riwayat Aktivitas Aktuator")
    perangkat_list = get_perangkat_list()
    perangkat_id = st.selectbox("🔧 Pilih Perangkat", perangkat_list)

    data = (
        supabase.table("aktuator_data")
        .select("*")
        .eq("perangkat_id", perangkat_id)
        .order("created_at", desc=True)
        .limit(200)
        .execute()
        .data
    )

    df = pd.DataFrame(data)

    if not df.empty:
        df["created_at"] = pd.to_datetime(df["created_at"], format='ISO8601')
        df = df.sort_values("created_at")

        st.subheader("📋 Tabel Status Aktuator")
        st.dataframe(df[::-1], use_container_width=True)

    else:
        st.warning("⚠️ Tidak ditemukan data aktuator untuk perangkat yang dipilih.")

def riwayat_feeding_log():
    st.subheader("🍽️ Riwayat Pemberian Pakan")
    perangkat_list = get_perangkat_list()
    perangkat_id = st.selectbox("🔧 Pilih Perangkat", perangkat_list)

    # Ambil usertank_id berdasarkan perangkat_id
    usertank_response = supabase.table("usertank").select("id").eq("perangkat_id", perangkat_id).execute()
    usertank_data = usertank_response.data
    if usertank_data:
      user_tank_id = usertank_data[0]['id']
      # Ambil data feeding log berdasarkan usertank_id
      data = (
          supabase.table("feeding_log")
          .select("*")
          .eq("user_tank_id", user_tank_id)
          .order("created_at", desc=True)
          .limit(200)
          .execute()
          .data
      )

      df = pd.DataFrame(data)

      if not df.empty:
          df["created_at"] = pd.to_datetime(df["created_at"], format='ISO8601')
          df = df.sort_values("created_at")

          st.subheader("📋 Tabel Log Pakan")
          st.dataframe(df[::-1], use_container_width=True)

      else:
          st.warning("⚠️ Tidak ada data feeding log untuk perangkat yang dipilih.")
    else:
      st.warning("❗ Tidak ditemukan usertank yang terhubung ke perangkat ini.")



def mengelola_tank():
    st.subheader('🗂️ Mengelola Tank')
    user = st.session_state.user
    status = 'tidak'
    select_count = 0

    def insert_usertank(user_id, perangkat_id, tank_id, nama_tank, guppy_id):
        supabase.table("usertank").insert({
            'user_id': user_id,
            'perangkat_id': perangkat_id,
            'tank_id': tank_id,
            'nama_tank': nama_tank,
            'guppy_id': guppy_id
        }).execute()

    def update_usertank(usertank_id, user_id, perangkat_id, tank_id, nama_tank, guppy_id):
        supabase.table("usertank").update({
            'user_id': user_id,
            'perangkat_id': perangkat_id,
            'tank_id': tank_id,
            'nama_tank': nama_tank,
            'guppy_id': guppy_id
        }).eq('id', usertank_id).execute()

    def delete_usertank(usertank_ids):
        for uid in usertank_ids:
            supabase.table("usertank").delete().eq('id', uid).execute()

    usertank = supabase.table("usertank").select("*").execute()
    editmode = st.toggle(label="Edit Mode")
    if editmode:
        status = 'aktif'
        st.markdown("### Mode Edit Aktif")

    df = pd.DataFrame(usertank.data)
    df['user_id'] = df['user_id'].astype(str)
    grid = table(df[['id', 'user_id', 'perangkat_id', 'tank_id', 'nama_tank', 'guppy_id']], status)
    selected_rows = grid['selected_rows']
    select_count = len(selected_rows) if selected_rows is not None else 0

    if editmode:
        perangkat_list = get_perangkat_list()
        users = get_users()
        perangkat = get_perangkat()
        tank_types = get_tank_types()
        guppy_types = get_guppy_types()

        user_options = {str(user['id']): user['nama'] for user in users}
        perangkat_options = {p['id']: p['nama'] for p in perangkat}
        tank_type_options = {tank['id']: tank['type_tank'] for tank in tank_types}
        guppy_type_options = {guppy['id']: guppy['jenis'] for guppy in guppy_types}

        if select_count == 0:
            st.subheader('➕ Tambah Data')
            selected_user_id = st.text_input("👤 Pilih User", user["id"], disabled=True)
            selected_perangkat = st.selectbox("📟 Pilih Perangkat", list(perangkat_options.values()))
            selected_tank_type = st.selectbox("🐟 Pilih Tipe Akuarium", list(tank_type_options.values()))
            input_nama_tank = st.text_input("🐟 Masukan Nama Tank")
            selected_guppy_type = st.selectbox("🐟 Pilih Jenis Guppy", list(guppy_type_options.values()))

            selected_perangkat_id = [pid for pid, name in perangkat_options.items() if name == selected_perangkat][0]
            selected_tank_type_id = [tid for tid, name in tank_type_options.items() if name == selected_tank_type][0]
            selected_guppy_type_id = [gid for gid, name in guppy_type_options.items() if name == selected_guppy_type][0]

            if st.button('💾 Tambah UserTank'):
                existing = supabase.table("usertank").select("id").eq("perangkat_id", selected_perangkat_id).execute()
                if existing.data:
                    st.error("❌ Perangkat ini sudah terdaftar di UserTank.")
                else:
                    insert_usertank(selected_user_id, selected_perangkat_id, selected_tank_type_id, input_nama_tank, selected_guppy_type_id)
                    st.success("✅ UserTank berhasil ditambahkan.")
                    st.toast("Perubahan Tank Tersimpan", icon="💾")
                    st.rerun()

        else:
            if select_count == 1:
                st.subheader("✏️ Edit Data")
                selected = selected_rows

                selected_user = st.selectbox("👤 Pilih User", list(user_options.values()), index=list(user_options.values()).index(user_options[selected['user_id'][0]]))
                selected_perangkat = st.selectbox("📟 Pilih Perangkat", list(perangkat_options.values()), index=list(perangkat_options.values()).index(perangkat_options[selected['perangkat_id'][0]]))
                selected_tank_type = st.selectbox("🐟 Pilih Tipe Akuarium", list(tank_type_options.values()), index=list(tank_type_options.values()).index(tank_type_options[selected['tank_id'][0]]))
                input_nama_tank = st.text_input("🐟 Masukan Nama Tank",value=selected['nama_tank'][0])
                selected_guppy_type = st.selectbox("🐟 Pilih Jenis Guppy", list(guppy_type_options.values()), index=list(guppy_type_options.values()).index(guppy_type_options[selected['guppy_id'][0]]))

                selected_user_id = [uid for uid, name in user_options.items() if name == selected_user][0]
                selected_perangkat_id = [pid for pid, name in perangkat_options.items() if name == selected_perangkat][0]
                selected_tank_type_id = [tid for tid, name in tank_type_options.items() if name == selected_tank_type][0]
                selected_guppy_type_id = [gid for gid, name in guppy_type_options.items() if name == selected_guppy_type][0]

                col1, col2 = st.columns(2)
                with col1:
                    if st.button("🗑️ Delete"):
                        delete_usertank([selected['id'][0]])
                        st.toast("Perubahan Tank Tersimpan", icon="💾")
                        st.success("✅ Data berhasil dihapus.")
                        st.rerun()
                with col2:
                    if st.button("✏️ Update"):
                        update_usertank(selected['id'][0], selected_user_id, selected_perangkat_id, selected_tank_type_id, input_nama_tank, selected_guppy_type_id)
                        st.toast("Perubahan Tank Tersimpan", icon="💾")
                        st.success("✅ Data berhasil diperbarui.")
                        st.rerun()

            elif select_count > 1:
                st.warning("🗑️ Mode hapus banyak aktif")
                if st.button(f"{select_count}🗑️ Hapus Semua "):
                    delete_usertank(selected_ids)
                    st.toast("Perubahan UserTank Tersimpan", icon="💾")
                    st.success("✅ Semua data terpilih berhasil dihapus.")
                    st.rerun()




def mengelola_user():
    st.subheader('🗂️ Mengelola User')
    status = 'tidak'
    select_count = 0
    def insert_user(nama, email, password, tipe):
      """Fungsi untuk menambahkan user baru ke dalam database."""
      response = (
          supabase.table("users")
          .insert({
              "nama": nama,
              "email": email,
              "password": password,
              "typeaccount": tipe
          })
          .execute()
      )


    def update_user(user_id, nama, email, password, tipe):
      """Fungsi untuk memperbarui data user di dalam database."""
      response = (
          supabase.table("users")
          .update({
              "nama": nama,
              "email": email,
              "password": password,
              "typeaccount": tipe
          })
          .eq('id', user_id)
          .execute()
      )


    def delete_user(user_ids):
      """Fungsi untuk menghapus user berdasarkan ID."""
      for user_id in user_ids:
          response = supabase.table("users").delete().eq('id', user_id).execute()

    # Mengaktifkan mode edit jika toggle aktif

    users = supabase.table("users").select("*").execute()
    editmode = st.toggle(label="Edit Mode")
    if editmode:
        status = 'aktif'
        st.markdown("### Mode Edit Aktif")

    df = pd.DataFrame(users.data)
    grid = table(df[['id', 'nama', 'email','password', 'typeaccount']], status)
    selected_rows = grid['selected_rows']

    if selected_rows is not None:
        select_count = len(selected_rows)
    else:
        select_count = 0

    if editmode:
        if select_count == 0:
          st.subheader('➕ Tambah Data')
          nama = st.text_input("Nama")
          email = st.text_input("Email")
          password = st.text_input("Password", type="password")
          tipe = st.selectbox("Tipe Akun", ["admin", "user"])
          if st.button("💾 Tambah UserTank"):
              if nama and email and password:
                  insert_user(nama, email, password, tipe)
                  st.toast("Perubahan User Tersimpan", icon="💾")
                  st.success("✅  berhasil ditambahkan.")
                  st.rerun()
              else:
                  st.warning("❗ Lengkapi semua data.")
        else:
          if select_count == 1:
            st.subheader("✏️ Edit User")
            selected = selected_rows

            # Edit data user
            selected_user = selected
            user_id = selected_user['id'][0]
            st.text_input("user_id", value=user_id, disabled=True)
            nama = st.text_input("Nama", value=selected_user['nama'][0])
            email = st.text_input("Email", value=selected_user['email'][0])
            password = st.text_input("Password", value=selected_user['password'][0])
            tipe = st.selectbox("Tipe Akun", ["admin", "user"], index=["admin", "user"].index(selected_user['typeaccount'][0]))

            col1, col2 = st.columns(2)
            with col1:
                if st.button("🗑️ Delete"):
                    delete_user([user_id])
                    st.toast("Perubahan User Tersimpan", icon="💾")
                    st.success("✅ Data berhasil dihapus.")
                    st.rerun()
            with col2:
                if st.button("✏️ Update"):
                    update_user(user_id, nama, email, password, tipe)
                    st.toast("Perubahan User Tersimpan", icon="💾")
                    st.success("✅ Data berhasil diperbarui.")
                    st.rerun()

          elif select_count > 1:
            st.warning("🗑️ Mode hapus banyak aktif")
            selected_ids = selected_rows['id']

            if st.button(f"{select_count}🗑️ Hapus Semua "):
                delete_user(selected_ids)
                st.toast("Perubahan User Tersimpan", icon="💾")
                st.success("✅ Semua data terpilih berhasil dihapus.")
                st.rerun()

def mengelola_tank_type():
    status = 'tidak'
    st.subheader("🗂️ Mengelola Tipe Akuarium")

    def insert_tank_type(type_tank):
        supabase.table("tank_type").insert({"type_tank": type_tank}).execute()

    def update_tank_type(id, type_tank):
        supabase.table("tank_type").update({"type_tank": type_tank}).eq("id", id).execute()

    def delete_tank_types(ids):
        for i in ids:
            supabase.table("tank_type").delete().eq("id", i).execute()

    tank_types = supabase.table("tank_type").select("*").execute()
    editmode = st.toggle(label="Edit Mode")
    if editmode:
        status = 'aktif'
        st.markdown("### Mode Edit Aktif")

    df = pd.DataFrame(tank_types.data)
    grid = table(df[['id', 'type_tank']], status)
    selected_rows = grid['selected_rows']

    if editmode:
        if selected_rows is not None:
            select_count = len(selected_rows)
        else:
            select_count = 0

        if select_count == 0:
            st.title('➕ Tambah Tipe Akuarium')
            type_tank = st.text_input("Nama Tipe Akuarium")
            if st.button("💾 Tambah Tipe"):
                if type_tank:
                    insert_tank_type(type_tank)
                    st.toast("Perubahan Tipe Akuarium Tersimpan", icon="💾")
                    st.success("✅ Tipe Akuarium berhasil ditambahkan.")
                    st.rerun()
                else:
                    st.warning("❗ Lengkapi nama tipe.")

        elif select_count == 1:
            st.title("✏️ Edit Tipe Akuarium")
            selected = selected_rows
            type_tank = st.text_input("Nama Tipe Akuarium", value=selected['type_tank'][0])
            col1, col2 = st.columns(2)
            with col1:
                if st.button("🗑️ Delete"):
                    delete_tank_types([selected['id'][0]])
                    st.toast("Perubahan Tipe Akuarium Tersimpan", icon="💾")
                    st.success("✅ Data berhasil dihapus.")
                    st.rerun()
            with col2:
                if st.button("✏️ Update"):
                    update_tank_type(selected['id'][0], type_tank)
                    st.toast("Perubahan Tipe Akuarium Tersimpan", icon="💾")
                    st.success("✅ Data berhasil diperbarui.")
                    st.rerun()

        elif select_count > 1:
            st.warning("🗑️ Mode hapus banyak aktif")
            selected_ids = [r['id'] for r in selected_rows]
            if st.button(f"{select_count}🗑️ Hapus Semua "):
                delete_tank_types(selected_ids)
                st.toast("Perubahan Tipe Akuarium Tersimpan", icon="💾")
                st.success("✅ Semua data terpilih berhasil dihapus.")
                st.rerun()

def mengelola_guppytype():
    status = 'tidak'
    st.subheader("🗂️ Mengelola Jenis Ikan")

    def insert_guppytype(jenis,deskripsi):
        supabase.table("guppy_type").insert({
            "jenis":jenis,
            "deskripsi":deskripsi,
            }).execute()

    def update_guppytype(id, jenis,deskripsi):
        supabase.table("guppy_type").update({"jenis": jenis,"deskripsi":deskripsi}).eq("id", id).execute()

    def delete_guppytype(ids):
        for i in ids:
            supabase.table("guppy_type").delete().eq("id", i).execute()

    guppytype = supabase.table("guppy_type").select("*").execute()
    editmode = st.toggle(label="Edit Mode")
    if editmode:
        status = 'aktif'
        st.markdown("### Mode Edit Aktif")

    df = pd.DataFrame(guppytype.data)
    grid = table(df[['id', 'jenis', 'deskripsi']], status)
    selected_rows = grid['selected_rows']

    if editmode:
        if selected_rows is not None:
            select_count = len(selected_rows)
        else:
            select_count = 0

        if select_count == 0:
            st.title('➕ Tambah Jenis Guppy')
            jenis = st.text_input("Jenis")
            deskripsi = st.text_input("Deskripsi")
            if st.button("💾 Tambah Perangkat"):
                if jenis and deskripsi:
                    insert_guppytype(jenis,deskripsi)
                    st.toast("Perubahan Tipe Guppy Tersimpan", icon="💾")
                    st.success("✅ Perangkat berhasil ditambahkan.")
                    st.rerun()
                else:
                    st.warning("❗ Lengkapi nama perangkat.")

        elif select_count == 1:
            st.title("✏️ Edit Perangkat")
            selected = selected_rows
            idperangkat = st.text_input("ID Guppy", value=selected['id'][0], disabled=True)
            jenis = st.text_input("Jenis", value=selected['jenis'][0])
            deskripsi = st.text_input("Deskripsi", value=selected['deskripsi'][0])
            col1, col2 = st.columns(2)
            with col1:
                if st.button("🗑️ Delete"):
                    delete_guppytype([selected['id'][0]])
                    st.toast("Perubahan Tipe Guppy Tersimpan", icon="💾")
                    st.success("✅ Data berhasil dihapus.")
                    st.rerun()
            with col2:
                if st.button("✏️ Update"):
                    update_guppytype(selected['id'][0], jenis, deskripsi)
                    st.toast("Perubahan Tipe Guppy Tersimpan", icon="💾")
                    st.success("✅ Data berhasil diperbarui.")
                    st.rerun()

        elif select_count > 1:
            st.warning("🗑️ Mode hapus banyak aktif")
            selected_ids = selected_rows['id']
            if st.button(f"{select_count}🗑️ Hapus Semua "):
                delete_guppytype(selected_ids)
                st.toast("Perubahan Tipe Guppy Tersimpan", icon="💾")
                st.success("✅ Semua data terpilih berhasil dihapus.")
                st.rerun()

def mengelola_perangkat():
    status = 'tidak'
    st.subheader("🗂️ Mengelola Perangkat")

    def insert_perangkat(id,nama,lokasi):
        supabase.table("perangkat").insert({
            "id": id,
            "nama":nama,
            "lokasi":lokasi,
            }).execute()

    def update_perangkat(id, nama,lokasi):
        supabase.table("perangkat").update({"nama": nama,"lokasi":lokasi}).eq("id", id).execute()

    def delete_perangkat(ids):
        for i in ids:
            supabase.table("perangkat").delete().eq("id", i).execute()

    perangkat = supabase.table("perangkat").select("*").execute()
    editmode = st.toggle(label="Edit Mode")
    if editmode:
        status = 'aktif'
        st.markdown("### Mode Edit Aktif")

    df = pd.DataFrame(perangkat.data)
    grid = table(df[['id', 'nama', 'lokasi']], status)
    selected_rows = grid['selected_rows']

    if editmode:
        if selected_rows is not None:
            select_count = len(selected_rows)
        else:
            select_count = 0

        if select_count == 0:
            st.title('➕ Tambah Perangkat')
            idperangkat = st.text_input("ID Perangkat")
            nama = st.text_input("Nama Perangkat")
            lokasi = st.text_input("Lokasi Perangkat")
            if st.button("💾 Tambah Perangkat"):
                if nama:
                    insert_perangkat(idperangkat,nama,lokasi)
                    st.toast("Perubahan Perangkat Tersimpan", icon="💾")
                    st.success("✅ Perangkat berhasil ditambahkan.")
                    st.rerun()
                else:
                    st.warning("❗ Lengkapi nama perangkat.")

        elif select_count == 1:
            st.title("✏️ Edit Perangkat")
            selected = selected_rows
            idperangkat = st.text_input("ID Perangkat", value=selected['id'][0], disabled=True)
            nama = st.text_input("Nama Perangkat", value=selected['nama'][0])
            lokasi = st.text_input("Lokasi Perangkat", value=selected['lokasi'][0])
            col1, col2 = st.columns(2)
            with col1:
                if st.button("🗑️ Delete"):
                    delete_perangkat([selected['id'][0]])
                    st.toast("Perubahan Perangkat Tersimpan", icon="💾")
                    st.success("✅ Data berhasil dihapus.")
                    st.rerun()
            with col2:
                if st.button("✏️ Update"):
                    update_perangkat(selected['id'][0], nama, lokasi)
                    st.toast("Perubahan Perangkat Tersimpan", icon="💾")
                    st.success("✅ Data berhasil diperbarui.")
                    st.rerun()

        elif select_count > 1:
            st.warning("🗑️ Mode hapus banyak aktif")
            selected_ids = selected_rows['id']
            if st.button(f"{select_count}🗑️ Hapus Semua "):
                delete_perangkat(selected_ids)
                st.toast("Perubahan Perangkat Tersimpan", icon="💾")
                st.success("✅ Semua data terpilih berhasil dihapus.")
                st.rerun()


def settings():
    user = st.session_state['user']
    st.subheader("⚙️ Pengaturan Akun")
    with st.form("settings_form"):
        nama = st.text_input("Nama", value=user['nama'])
        email = st.text_input("Email", value=user['email'])
        password = st.text_input("Password", type="password", value=user['password'])
        submit = st.form_submit_button("Simpan Perubahan")

    if submit:
      if nama and email and password:
          # Update ke Supabase
          supabase.table("users").update({
              "nama": nama,
              "email": email,
              "password": password
          }).eq("email", user['email']).execute()

          # Ambil ulang data terbaru dari Supabase
          updated_user = supabase.table("users").select("*").eq("email", email).eq("password", password).execute()

          if updated_user.data:
              st.session_state.user = updated_user.data[0]
              st.toast("Perubahan Akun Tersimpan", icon="💾")
              st.success("✅ Perubahan akun berhasil disimpan.")
          else:
              st.error("⚠️ Gagal mengambil data pengguna setelah update.")
      else:
          st.warning("❗ Lengkapi semua data.")

def logout():
    st.subheader("⍈ Logout")
    if st.button("Logout"):
      st.session_state.clear()
      st.rerun()

# Main App
def main():
    # --- PERUBAHAN TEMA DIMULAI DI SINI ---

    # 1. Konfigurasi Tema Halaman (st.set_page_config)
    # Mengatur tema dasar menjadi 'light'.
    st.set_page_config(page_title="Sistem Monitoring Guppy", page_icon="🐟")

    # 2. Styles untuk option_menu disesuaikan untuk Tema Terang
    menu_styles = {
        "container": {
            "padding": "0!important",
            "background-color": "#F0F2F6", # Warna background sidebar terang
        },
        "icon": {
            "color": "#262730",  # Warna ikon gelap
            "font-size": "18px"
        },
        "nav-link": {
            "font-size": "16px",
            "text-align": "left",
            "margin": "4px",
            "padding": "8px 16px",
            "border-radius": "6px",
            "color": "#262730", # Warna teks gelap
        },
        "nav-link:hover": {
            "background-color": "#D3D8E2",  # highlight saat hover
            "color": "#262730"
        },
        "nav-link-selected": {
            "background-color": "#FFFFFF", # Background item terpilih
            "font-weight": "bold",
            "color": "#f63366", # Warna teks item terpilih (primary color)
            "border-left": "4px solid #f63366"  # Aksen warna primer
        }
    }
    # --- PERUBAHAN TEMA SELESAI DI SINI ---

    apply_custom_css()
    if 'logged_in' not in st.session_state:
        st.session_state.logged_in = False

    if 'selected_page' not in st.session_state:
        st.session_state.selected_page = 'Dashboard'

    if st.session_state.logged_in:
        user = st.session_state.user

        with st.sidebar:
          st.logo(
            logo_wide,
            icon_image=logo_wide,size="large"
        )
          st.caption(f"👋 Haii Selamat Datang {user['nama']} ({user.get('typeaccount', 'Unknown')})")

          if user['typeaccount'] == 'admin':
              main_page = option_menu(
                  menu_title="",
                  options=["Dashboard", "Jadwal Pakan", "Riwayat Data", "Manajemen Data", "Pengaturan", "Logout"],
                  icons=["house", "calendar", "bi bi-clock-history", "list-task", "gear", "bi bi-box-arrow-right"],
                  menu_icon="cast",
                  default_index=0,
                  styles=menu_styles
              )
              if main_page == "Riwayat Data":
                  submenu_page = option_menu(
                      menu_title="",
                      options=["Riwayat Sensor", "Riwayat Aktuator", "Riwayat Feeding"],
                      icons=["activity", "cpu", "list-check"],
                      menu_icon="clock-history",
                      default_index=0,
                      styles=menu_styles
                  )
                  selected_page = submenu_page

              elif main_page == "Manajemen Data":
                  submenu_page = option_menu(
                      menu_title="",
                      options=["Mengelola Tank", "Mengelola User", "Mengelola Tank Type", "Mengelola Guppy Type", "Mengelola Perangkat"],
                      icons=["bi bi-box2", "person", "bi bi-moisture", "bi bi-sliders2", "cpu"],
                      menu_icon="tools",
                      default_index=0,
                      styles=menu_styles
                  )
                  selected_page = submenu_page
              else:
                  selected_page = main_page
          else: # User Biasa
              main_page = option_menu(
                  menu_title="",
                  options=["Dashboard", "Jadwal Pakan", "Riwayat Data","Pengaturan", "Logout"],
                  icons=["house", "calendar", "bi bi-clock-history", "gear","bi bi-box-arrow-right"],
                  menu_icon="cast",
                  default_index=0,
                  orientation="vertical",
                  styles=menu_styles
              )
              if main_page == "Riwayat Data":
                  submenu_page = option_menu(
                      menu_title="",
                      options=["Riwayat Sensor", "Riwayat Aktuator", "Riwayat Feeding"],
                      icons=["activity", "cpu", "list-check"],
                      menu_icon="clock-history",
                      default_index=0,
                      styles=menu_styles
                  )
                  selected_page = submenu_page
              else:
                  selected_page = main_page

        st.session_state.selected_page = selected_page

        if selected_page == "Dashboard":
            if user['typeaccount'] == 'admin':
                admin_dashboard()
            else:
                user_dashboard()
        elif selected_page == "Jadwal Pakan":
          atur_jadwal_pakan()
        elif selected_page == "Riwayat Sensor":
          riwayat_sensor()
        elif selected_page == "Riwayat Aktuator":
          riwayat_aktuator()
        elif selected_page == "Riwayat Feeding":
          riwayat_feeding_log()
        elif selected_page == "Mengelola Tank":
          mengelola_tank()
        elif selected_page == "Mengelola Perangkat":
          mengelola_perangkat()
        elif selected_page == "Mengelola User":
          mengelola_user()
        elif selected_page == "Mengelola Tank Type":
          mengelola_tank_type()
        elif selected_page == "Mengelola Guppy Type":
          mengelola_guppytype()
        elif selected_page == "Pengaturan":
            settings()
        elif selected_page == "Logout":
            logout()

    else:
        login()

if __name__ == "__main__":
    main()
